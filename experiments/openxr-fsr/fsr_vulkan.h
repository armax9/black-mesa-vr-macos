#pragma once
#include "fsr_plan.h"
#include "fsr_spv.h"
// Included after VulkanDispatch. Experimental CPU-bridge projection path only.
namespace bmvr_fsr {
template<class VulkanDispatch>
class VulkanFsr {
    struct Image { VkImage image{};VkDeviceMemory memory{};VkImageView storage{},sampled{};VkImageLayout layout=VK_IMAGE_LAYOUT_UNDEFINED; };
    VkDevice device{};VulkanDispatch* vk{};
    PFN_vkCreateComputePipelines createCompute{};PFN_vkCmdDispatch dispatch{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkSampler sampler{};
    VkPipelineLayout pipelineLayout{};VkPipeline pipelines[2]{};
    VkDescriptorSet sets[2][2]{};Image images[2][2]{};
    uint32_t inputW{},inputH{},outputW{},outputH{};bool srgb{};bool attempted{};bool ready{};
    uint32_t recordedFrame[2]{};bool haveRecordedFrame[2]{};
    void Barrier(VkCommandBuffer cmd,Image& image,VkImageLayout next,VkAccessFlags dst,VkPipelineStageFlags stage) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.image=image.image;b.oldLayout=image.layout;b.newLayout=next;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        b.srcAccessMask=image.layout==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask=dst;
        vk->vkCmdPipelineBarrier(cmd,image.layout==VK_IMAGE_LAYOUT_UNDEFINED?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,stage,0,0,nullptr,0,nullptr,1,&b);
        image.layout=next;
    }
public:
    // At first init, allocate BOTH eyes or fail the session. No half-pair fallback.
    // Resize requires restart; no destruction/reallocation while GPU work is live.
    bool Prepare(VulkanDispatch& api,VkDevice d,VkPhysicalDevice physical,VkInstance instance,
                 uint32_t family,const VkPhysicalDeviceMemoryProperties& memory,
                 uint32_t inW,uint32_t inH,uint32_t outW,uint32_t outH,bool isSrgb) {
        if(attempted) return ready&&inputW==inW&&inputH==inH&&outputW==outW&&outputH==outH&&srgb==isSrgb;
        attempted=true;vk=&api;device=d;inputW=inW;inputH=inH;outputW=outW;outputH=outH;srgb=isSrgb;
        if(!inW||!inH||inW>outW||inH>outH||outW>4096||outH>4096) return false;
        createCompute=reinterpret_cast<PFN_vkCreateComputePipelines>(api.vkGetDeviceProcAddr(d,"vkCreateComputePipelines"));
        dispatch=reinterpret_cast<PFN_vkCmdDispatch>(api.vkGetDeviceProcAddr(d,"vkCmdDispatch"));
        auto getFormat=reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(api.vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFormatProperties"));
        if(!createCompute||!dispatch||!getFormat)return false;
        uint32_t count=0;api.vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
        std::vector<VkQueueFamilyProperties> families(count);api.vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
        if(family>=count||!(families[family].queueFlags&VK_QUEUE_COMPUTE_BIT))return false;
        VkFormatProperties f{};getFormat(physical,VK_FORMAT_R8G8B8A8_UNORM,&f);
        constexpr auto required=VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((f.optimalTilingFeatures&required)!=required)return false;
        if(srgb){getFormat(physical,VK_FORMAT_R8G8B8A8_SRGB,&f);if((f.optimalTilingFeatures&(VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))!=(VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))return false;}
        // The upload is BGRA, while reconstruction storage is RGBA. Check
        // the actual raw input sampling format as well as output storage.
        getFormat(physical,VK_FORMAT_B8G8R8A8_UNORM,&f);
        constexpr auto sampled=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((f.optimalTilingFeatures&sampled)!=sampled)return false;
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0]={0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        bindings[1]={1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=2;li.pBindings=bindings;
        if(api.vkCreateDescriptorSetLayout(d,&li,nullptr,&setLayout)!=VK_SUCCESS)return false;
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(EasuConstants)};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&range;
        if(api.vkCreatePipelineLayout(d,&pl,nullptr,&pipelineLayout)!=VK_SUCCESS)return false;
        VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,4}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=4;pi.poolSizeCount=2;pi.pPoolSizes=sizes;
        if(api.vkCreateDescriptorPool(d,&pi,nullptr,&pool)!=VK_SUCCESS)return false;
        VkDescriptorSetLayout layouts[4]={setLayout,setLayout,setLayout,setLayout};
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=pool;ai.descriptorSetCount=4;ai.pSetLayouts=layouts;
        VkDescriptorSet flatSets[4]{};
        if(api.vkAllocateDescriptorSets(d,&ai,flatSets)!=VK_SUCCESS)return false;
        for(unsigned e=0;e<2;++e)for(unsigned p=0;p<2;++p)sets[e][p]=flatSets[e*2+p];
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if(api.vkCreateSampler(d,&si,nullptr,&sampler)!=VK_SUCCESS)return false;
        for(unsigned p=0;p<2;++p){
            VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};mi.codeSize=p?sizeof(rcasSpv):sizeof(easuSpv);mi.pCode=p?rcasSpv:easuSpv;
            VkShaderModule module{};if(api.vkCreateShaderModule(d,&mi,nullptr,&module)!=VK_SUCCESS)return false;
            VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module;ci.stage.pName="main";ci.layout=pipelineLayout;
            const auto result=createCompute(d,VK_NULL_HANDLE,1,&ci,nullptr,&pipelines[p]);api.vkDestroyShaderModule(d,module,nullptr);
            if(result!=VK_SUCCESS)return false;
        }
        for(unsigned e=0;e<2;++e)for(unsigned p=0;p<2;++p){
            auto& i=images[e][p];VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_R8G8B8A8_UNORM;
            ci.extent={outW,outH,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
            if(api.vkCreateImage(d,&ci,nullptr,&i.image)!=VK_SUCCESS)return false;
            VkMemoryRequirements req{};api.vkGetImageMemoryRequirements(d,i.image,&req);uint32_t type=UINT32_MAX;
            for(uint32_t t=0;t<memory.memoryTypeCount;++t)if((req.memoryTypeBits&(1u<<t))&&(memory.memoryTypes[t].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=t;break;}
            if(type==UINT32_MAX)return false;
            VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=req.size;ma.memoryTypeIndex=type;
            if(api.vkAllocateMemory(d,&ma,nullptr,&i.memory)!=VK_SUCCESS||api.vkBindImageMemory(d,i.image,i.memory,0)!=VK_SUCCESS)return false;
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=i.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=VK_FORMAT_R8G8B8A8_UNORM;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            if(api.vkCreateImageView(d,&vi,nullptr,&i.storage)!=VK_SUCCESS)return false;
            // EASU->RCAS stays perceptual UNORM. Only final warp view decodes sRGB.
            vi.format=p==1&&srgb?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM;
            if(api.vkCreateImageView(d,&vi,nullptr,&i.sampled)!=VK_SUCCESS)return false;
        }
        ready=true;return true;
    }
    VkImageView Record(VkCommandBuffer cmd,unsigned eye,VkImageView input,uint32_t frame) {
        if(!ready||eye>=2||!input)return VK_NULL_HANDLE;
        // The runtime can submit the same game pair more than once. Reuse the
        // completed reconstruction; do not run EASU/RCAS at headset rate.
        if(haveRecordedFrame[eye]&&recordedFrame[eye]==frame)return images[eye][1].sampled;
        EasuConstants easu{};RcasConstants rcas{};
        FsrEasuCon(easu.con[0],easu.con[1],easu.con[2],easu.con[3],float(inputW),float(inputH),float(inputW),float(inputH),float(outputW),float(outputH));
        FsrRcasCon(rcas.con,.2f);
        easu.extent[0]=rcas.extent[0]=outputW;easu.extent[1]=rcas.extent[1]=outputH;easu.extent[2]=0; // Input is already an encoded UNORM view, even for sRGB output.
        for(unsigned p=0;p<2;++p){
            auto& dest=images[eye][p];Barrier(cmd,dest,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            VkDescriptorImageInfo sampled{sampler,p?images[eye][0].sampled:input,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo storage{VK_NULL_HANDLE,dest.storage,VK_IMAGE_LAYOUT_GENERAL};
            VkWriteDescriptorSet writes[2]{};
            for(unsigned j=0;j<2;++j){writes[j]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[j].dstSet=sets[eye][p];writes[j].dstBinding=j;writes[j].descriptorCount=1;writes[j].descriptorType=j?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[j].pImageInfo=j?&storage:&sampled;}
            vk->vkUpdateDescriptorSets(device,2,writes,0,nullptr);
            vk->vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[p]);
            vk->vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout,0,1,&sets[eye][p],0,nullptr);
            vk->vkCmdPushConstants(cmd,pipelineLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,p?sizeof(rcas):sizeof(easu),p?static_cast<const void*>(&rcas):static_cast<const void*>(&easu));
            dispatch(cmd,(outputW+7)/8,(outputH+7)/8,1);
            Barrier(cmd,dest,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,p?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }
        recordedFrame[eye]=frame;haveRecordedFrame[eye]=true;
        return images[eye][1].sampled;
    }
    void InvalidateFrameCache() {
        haveRecordedFrame[0]=haveRecordedFrame[1]=false;
    }
    VkImage OutputImage(unsigned eye)const{return eye<2?images[eye][1].image:VK_NULL_HANDLE;}
    void Shutdown(){
        if(!device||!vk)return;
        for(auto& pair:images)for(auto& i:pair){if(i.sampled)vk->vkDestroyImageView(device,i.sampled,nullptr);if(i.storage)vk->vkDestroyImageView(device,i.storage,nullptr);if(i.image)vk->vkDestroyImage(device,i.image,nullptr);if(i.memory)vk->vkFreeMemory(device,i.memory,nullptr);i={};}
        for(auto& p:pipelines){if(p)vk->vkDestroyPipeline(device,p,nullptr);p={};}
        if(sampler)vk->vkDestroySampler(device,sampler,nullptr);
        if(pool)vk->vkDestroyDescriptorPool(device,pool,nullptr);
        if(pipelineLayout)vk->vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
        if(setLayout)vk->vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
        sampler={};pool={};pipelineLayout={};setLayout={};
        for(auto& pair:sets)for(auto& set:pair)set={};
        InvalidateFrameCache();ready=false;attempted=false;
        device={};vk=nullptr;
    }
};
}
