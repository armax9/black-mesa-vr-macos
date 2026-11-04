import importlib.util,json,hashlib,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('installer',Path(__file__).parents[1]/'packaging/macos/install.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class InstallChecks(unittest.TestCase):
 def setup_game(self,base):
  game=base/'Black Mesa';payload=base/'package';game.mkdir();payload.mkdir();(game/'bms.exe').write_bytes(b'game')
  files={'d3d9.dll':b'new-dll','VR/openxr_helper64/OpenXRHelper64.exe':b'new-helper','VR/config.txt':b'FSR=false\nRenderScale=1.0\nVrLaserBeam=false\n','VR/openxr_helper64/openxr_loader.dll':b'new-loader'}
  for rel,data in files.items():
   p=payload/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
  (payload/'MANIFEST.json').write_text(json.dumps({'version':'test','files':{p:hashlib.sha256(d).hexdigest() for p,d in files.items()}}))
  old={'d3d9.dll':b'old-dll','VR/openxr_helper64/OpenXRHelper64.exe':b'old-helper','VR/config.txt':b'FSR=true\nRenderScale=0.77\n','VR/openxr_helper64/openxr_loader.dll':b'old-loader'}
  for rel,data in old.items():
   p=game/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
  return game,payload,old
 def test_preserve_and_backup(self):
  with tempfile.TemporaryDirectory() as t:
   g,p,old=self.setup_game(Path(t))
   with patch.object(mod.subprocess,'check_output',return_value=''):mod.install(g,p)
   self.assertEqual((g/'d3d9.dll').read_bytes(),b'new-dll');self.assertEqual((g/'VR/openxr_helper64/OpenXRHelper64.exe').read_bytes(),b'new-helper')
   self.assertEqual((g/'VR/openxr_helper64/openxr_loader.dll').read_bytes(),b'old-loader')
   cfg=(g/'VR/config.txt').read_text();self.assertIn('FSR=true',cfg);self.assertIn('RenderScale=0.77',cfg);self.assertIn('VrLaserBeam=false',cfg)
   backup=next((g/'VR/backups').iterdir());records=json.loads((backup/'restore.json').read_text())['files']
   for item in records:self.assertEqual((backup/'objects'/item['before_sha256']).read_bytes(),old[item['path']])
 def test_failure_rolls_back(self):
  with tempfile.TemporaryDirectory() as t:
   g,p,old=self.setup_game(Path(t));real=mod.os.replace;count=[0]
   def fail(src,dst):
    count[0]+=1
    if count[0]==2:raise OSError('simulated interruption')
    return real(src,dst)
   with patch.object(mod.subprocess,'check_output',return_value=''),patch.object(mod.os,'replace',side_effect=fail):
    with self.assertRaises(OSError):mod.install(g,p)
   for rel,data in old.items():self.assertEqual((g/rel).read_bytes(),data)
if __name__=='__main__':unittest.main()
