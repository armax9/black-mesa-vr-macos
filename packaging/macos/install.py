#!/usr/bin/env python3
"""Install Black Mesa VR for macOS. Existing player options are preserved."""
from pathlib import Path
import argparse,datetime,hashlib,json,os,re,shutil,subprocess,sys

def digest(data):return hashlib.sha256(data).hexdigest()
def choose_game():
 result=subprocess.run(['osascript','-e','POSIX path of (choose folder with prompt "Select the Black Mesa folder containing bms.exe")'],capture_output=True,text=True)
 if result.returncode:raise SystemExit('Installation cancelled.')
 return Path(result.stdout.strip())
def install(game,payload,backup_root=None):
 game=game.resolve();assert (game/'bms.exe').is_file(),'Select the Black Mesa folder containing bms.exe.'
 proc=subprocess.check_output(['ps','-axo','comm='],text=True)
 if any(re.search(r'(?:^|[/\\])(?:bms\.exe|OpenXRHelper64\.exe)(?:\s|$)',line,re.I) for line in proc.splitlines()):raise RuntimeError('Close Black Mesa before installing.')
 manifest=json.loads((payload/'MANIFEST.json').read_text())
 for rel,sha in manifest['files'].items():
  path=Path(rel)
  assert not path.is_absolute() and '..' not in path.parts,'Unsafe manifest path'
  assert digest((payload/path).read_bytes())==sha,'Package verification failed: '+rel
 backup=(backup_root or game/'VR/backups')/(manifest['version']+'-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
 objects=backup/'objects';objects.mkdir(parents=True)
 keep={'VR/viewmodel_offsets.txt','bms/cfg/bmvr_performance.cfg','VR/openxr_helper64/openxr_loader.dll','openvr_api.dll','bin/openvr_api.dll','bin/thirdparty/dxvk-windows-x86/openvr_api.dll'}
 changes=[];preserved=[]
 try:
  for rel in manifest['files']:
   dst=game/rel;data=(payload/rel).read_bytes()
   if rel in keep and dst.is_file():preserved.append(rel);continue
   if rel=='VR/config.txt' and dst.is_file():
    current=dst.read_text();keys=set(re.findall(r'^\s*([A-Za-z][\w]*)\s*=',current,re.M));extra=[]
    for line in data.decode().splitlines():
     match=re.match(r'^\s*([A-Za-z][\w]*)\s*=',line)
     if match and match[1] not in keys:extra.append(line);keys.add(match[1])
    if not extra:continue
    data=(current.rstrip()+'\n\n# New macOS VR options.\n'+'\n'.join(extra)+'\n').encode()
   before=dst.read_bytes() if dst.is_file() else None
   if before==data:continue
   old_hash=digest(before) if before is not None else None
   if old_hash and not (objects/old_hash).exists():(objects/old_hash).write_bytes(before)
   item={'path':rel,'before_sha256':old_hash,'after_sha256':digest(data)};changes.append(item)
   (backup/'restore.json').write_text(json.dumps({'version':manifest['version'],'files':changes},indent=2)+'\n')
   dst.parent.mkdir(parents=True,exist_ok=True);temporary=dst.with_name(dst.name+'.bmvr-install-tmp');temporary.write_bytes(data);os.replace(temporary,dst)
   assert digest(dst.read_bytes())==item['after_sha256'],rel
 except Exception:
  for item in reversed(changes):
   dst=game/item['path']
   dst.with_name(dst.name+'.bmvr-install-tmp').unlink(missing_ok=True)
   if item['before_sha256']:shutil.copy2(objects/item['before_sha256'],dst)
   else:dst.unlink(missing_ok=True)
  raise
 print('Installed '+manifest['version']+'. Your existing mod settings were preserved.')
 print('Backup: '+str(backup))
 print('Start MoltenVR, then click Play in Windows Steam through CrossOver.')
 return changes
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('game',nargs='?',type=Path);args=parser.parse_args()
 try:install(args.game or choose_game(),Path(__file__).resolve().parent/'Game')
 except (AssertionError,RuntimeError,OSError) as e:print('Installation failed: '+str(e),file=sys.stderr);sys.exit(1)
