#pragma once
#include <d3d9.h>

// D3D9 StateBlock::Capture intentionally does NOT refresh stream offsets.
// A cached block can restore a new VB with its creation-time offset. Preserve
// these explicitly around every custom draw that uses persistent blocks.
class VrD3D9StreamSnapshot {
public:
    VrD3D9StreamSnapshot()=default;
    VrD3D9StreamSnapshot(const VrD3D9StreamSnapshot&)=delete;
    VrD3D9StreamSnapshot& operator=(const VrD3D9StreamSnapshot&)=delete;
    ~VrD3D9StreamSnapshot(){Release();}
    bool Capture(IDirect3DDevice9* device,UINT count=16) {
        Release();m_Device=device;m_Count=count<=16?count:16;
        if(!m_Device)return false;
        for(UINT i=0;i<m_Count;++i) {
            if(FAILED(m_Device->GetStreamSource(i,&m_Streams[i].buffer,&m_Streams[i].offset,&m_Streams[i].stride))) {
                Release();return false;
            }
        }
        return true;
    }
    void Restore() {
        if(m_Device)for(UINT i=0;i<m_Count;++i)
            m_Device->SetStreamSource(i,m_Streams[i].buffer,m_Streams[i].offset,m_Streams[i].stride);
        Release();
    }
private:
    struct Stream {IDirect3DVertexBuffer9* buffer=nullptr;UINT offset=0,stride=0;};
    Stream m_Streams[16]{};
    IDirect3DDevice9* m_Device=nullptr;
    UINT m_Count=0;
    void Release(){
        for(auto& s:m_Streams){if(s.buffer)s.buffer->Release();s=Stream{};}
        m_Device=nullptr;m_Count=0;
    }
};
