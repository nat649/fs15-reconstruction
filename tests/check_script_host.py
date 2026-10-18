"""Installed script-host transport checks using authored Lua/XML only."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

LABELS=['Button_Career','Button_Multiplayer','Button_Tutorials','ButtonAchievements','Button_Settings','Button_Credits','Button_Quit']
# Authored fixtures, not copied game scripts; no claim of recovered-code parity.
CLASS='function Class(t) return {__index=t} end\n'
UTILS='Utils = { getNoNil = function(value, default) if value == nil then return default end return value end }\n'
CATALOG='''I18N = {}
function I18N:new()
  local item = setmetatable({texts={},descriptions={}}, {__index=self})
  local function read(file, path, target)
    local handle = loadXMLFile('authored', file)
    local index = 0
    while hasXMLProperty(handle, path..'('..index..')') do
      local key = path..'('..index..')'
      target[getXMLString(handle,key..'#name')] = getXMLString(handle,key..'#text')
      index = index + 1
    end
    delete(handle)
  end
  read('dataS/l10n'..g_languageSuffix..'.xml','l10n.texts.text',item.texts)
  read('dataS/descriptions'..g_languageSuffix..'.xml','descriptions.description',item.descriptions)
  item.fluidFactor, item.powerFactorHP, item.powerFactorKW = 1, 1, 0.735499
  return item
end
function I18N:hasText(key) return self.texts[key] ~= nil end
function I18N:getText(key) return self.texts[key] end
'''
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--exe',type=Path,required=True);p.add_argument('--workdir',type=Path,required=True);p.add_argument('--report',type=Path);a=p.parse_args()
    exe=a.exe.resolve();a.workdir.mkdir(parents=True,exist_ok=True);checks=[]
    with tempfile.TemporaryDirectory(prefix='script-host-',dir=a.workdir.resolve()) as directory:
        cwd=Path(directory);root=cwd/'local resources';scripts=root/'lua/dataS/scripts';assets=root/'assets/dataS';(scripts/'shared').mkdir(parents=True);assets.mkdir(parents=True)
        for name,body in [('shared/class.lua',CLASS),('Utils.lua',UTILS),('I18N.lua',CATALOG)]: (scripts/name).write_text(body,encoding='utf-8')
        xml='<l10n><texts>'+''.join(f'<text name="{name}" text="authored {index}"/>' for index,name in enumerate(LABELS))+'</texts></l10n>'
        (assets/'l10n_en.xml').write_text(xml);(assets/'descriptions_en.xml').write_text('<descriptions><description name="authored" text="fixture"/></descriptions>')
        before={str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in root.rglob('*') if f.is_file()}
        def run(*args,code=0):
            r=subprocess.run([str(exe),*map(str,args)],cwd=cwd,capture_output=True,text=True,encoding='utf-8')
            assert r.returncode==code,(args,r.returncode,r.stderr);return r
        assert 'Text-only' in run('--help').stdout;checks.append('installed_help_from_other_working_directory')
        result=json.loads(run('--i18n-probe','--root',root).stdout)
        assert result['source_files']==3 and result['text_entries']==7 and result['description_entries']==1 and result['main_menu_labels']==7
        assert result['retained_xml_documents']==0 and not result['native_engine_started'] and not result['renderer_started'] and not result['playable'];checks.append('authored_source_and_xml_bootstrap_with_cleanup')
        after={str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in root.rglob('*') if f.is_file()};assert before==after;checks.append('source_and_resource_files_unchanged')
        for args in [('--reference-bytecode',),('--i18n-probe',),('--i18n-probe','--root',root,'--language','../en')]:run(*args,code=2)
        checks.append('invalid_mode_and_language_reject')
        source=scripts/'I18N.lua';source.write_bytes(b'\x1bLJauthored-invalid-bytecode');r=run('--i18n-probe','--root',root,code=1);assert 'Refusing bytecode' in r.stderr;source.write_text(CATALOG);checks.append('bytecode_refused_in_runtime')
        resource=assets/'descriptions_en.xml';resource.unlink();run('--i18n-probe','--root',root,code=1);checks.append('missing_resource_fails_without_fallback')
        evidence=dict(status='passed',executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),lua_runtime=result['lua_runtime'],xml_backend=result['xml_backend'],checks=checks,
            original_assets_used=False,native_engine_started=False,renderer_started=False,playable=False,scope='Authored source fixtures validate installed Lua/XML text-loader transport; original recovered-source parity is a separate local test.')
    if a.report:a.report.write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(evidence,indent=2))
if __name__=='__main__':main()
