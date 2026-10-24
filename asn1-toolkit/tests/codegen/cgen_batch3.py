"""Real C artifact shape, publication, cache and independent build acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


def main():
    p=argparse.ArgumentParser()
    for key in ('compiler','cc','cxx','runtime','root','work','case','cmake'):
        p.add_argument('--'+key,required=True)
    p.add_argument('--sanitizers',action='store_true')
    a=p.parse_args()
    root=Path(a.root).resolve()
    Path(a.work).mkdir(parents=True,exist_ok=True)
    san=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=undefined'] if a.sanitizers else []
    def run(command,success=True):
        result=subprocess.run(list(map(str,command)),capture_output=True,text=True)
        assert (result.returncode==0)==success,(command,result.returncode,result.stdout,result.stderr)
        return result
    with tempfile.TemporaryDirectory(prefix='C artifacts ',dir=a.work) as tmp:
        work=Path(tmp)
        fixture=root/'tests/fixtures/cgen_batch2.asn'
        def emit(out,flags=(),schema=None,success=True):
            return run([a.compiler,'--lang','c','--types-only','--c-prefix','stage3',
                        '--emit-dir',out,*flags,schema or fixture],success)
        def snapshot(out):
            return {f.relative_to(out).as_posix():f.read_bytes() for f in out.rglob('*') if f.is_file()}
        def manifest(out):
            lines=(out/'.asn1cxx-manifest').read_text().splitlines()
            assert lines[0]=='asn1cxx-artifacts-v1'
            owned={}
            for line in lines[1:]:
                if line.startswith('dependencies '): continue
                name,digest=line.split()
                assert Path(name).name==name
                assert hashlib.sha256((out/name).read_bytes()).hexdigest()==digest
                owned[name]=digest
            return owned
        def build(out,label):
            data=json.loads((out/'generated-sources.json').read_text())
            assert data['language']=='c' and data['types_only'] is True
            for unit in data['units']:
                assert unit['bytes']==len((out/unit['file']).read_bytes())
            assert len(data['sources'])==len(set(data['sources']))
            definitions=[]
            for unit in data['units']:
                if unit['file'] not in data['sources'] and label!='inline': continue
                text=(out/unit['file']).read_text()
                found=re.findall(r'^(?:static inline )?(?:void|asn1_status_t|size_t|const stage3_field_plan\s*\*)\s*(stage3_\w+)\([^;\n]*\)\s*\{',text,re.M)
                assert len(found)==unit['functions'],(unit,len(found))
                definitions.extend(found)
            assert len(definitions)==len(set(definitions))
            sources=[]
            # Test-only wrappers intercept allocations in every source shard.
            for index,name in enumerate(data['sources']):
                wrapper=work/(label+str(index)+'.c')
                wrapper.write_text('#include <stdlib.h>\n'
                    'void *stage3_test_malloc(size_t);\nvoid *stage3_test_calloc(size_t,size_t);\n'
                    'void stage3_test_free(void*);\n#define malloc stage3_test_malloc\n'
                    '#define calloc stage3_test_calloc\n#define free stage3_test_free\n'
                    '#include "'+name+'"\n',encoding='utf-8')
                sources.append(wrapper)
            second=work/(label+'second.c')
            second.write_text('#include "generated.h"\nint second(void){stage3_mA_tFlag v;'
                              'stage3_mA_tFlag_init(&v);return v.value;}\n',encoding='utf-8')
            exe=work/(label+'run')
            command=[a.cc,'-std=c11','-pedantic-errors','-Wall','-Wextra','-Werror',*san,
                     '-I',root/'include','-I',out,*sources,
                     root/'tests/codegen/cgen_batch3_driver.c',second,a.runtime,'-o',exe]
            rss=work/(label+'rss.txt')
            if sys.platform.startswith('linux') and Path('/usr/bin/time').exists():
                command=['/usr/bin/time','-f','%M','-o',rss,*command]
            begin=time.perf_counter();run(command);elapsed=time.perf_counter()-begin
            result=run([exe]).stdout
            assert result=='87\n'
            # GCC UBSan itself depends on libstdc++; check released C binaries,
            # not the instrumentation toolchain's transitive libraries.
            if sys.platform.startswith('linux') and not a.sanitizers:
                deps=run(['ldd',exe]).stdout
                assert 'libstdc++' not in deps and 'libc++' not in deps
            cpp=work/(label+'header.cpp');cpp.write_text('#include "generated.h"\n',encoding='utf-8')
            run([a.cxx,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic-errors','-fsyntax-only',
                 '-I',root/'include','-I',out,cpp])
            return {'shape':label,'sources':len(data['sources']),
                    'max_source_bytes':max((len((out/n).read_bytes()) for n in data['sources']),default=0),
                    'header_bytes':len((out/'generated.h').read_bytes()),
                    'max_source_functions':max((u['functions'] for u in data['units'] if u['file'] in data['sources']),default=0),
                    'compile_link_seconds':round(elapsed,3),'peak_child_rss_kib':int(rss.read_text()) if rss.exists() else None}
        if a.case=='shapes':
            metrics=[]
            for label,flags in [('outline',[]),('inline',['--emit-inline']),('modules',['--emit-split']),
                                ('shards',['--split-max-types','8']),
                                ('module_shards',['--emit-split','--split-max-types','8'])]:
                out=work/label;emit(out,flags);manifest(out)
                data=json.loads((out/'generated-sources.json').read_text())
                assert len(data['sources'])==0 if label=='inline' else len(data['sources'])>0
                if 'shards' in label: assert len(data['sources'])>2
                metrics.append(build(out,label))
            Path(a.work,'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
            print(json.dumps(metrics,indent=2))
        elif a.case=='single_module':
            schema=work/'solo.asn'
            schema.write_text('Solo DEFINITIONS ::= BEGIN\n'+''.join(
                f'Type{i} ::= BOOLEAN\n' for i in range(40))+'END\n')
            out=work/'solo'
            for budget,count in ((8,5),(13,4)):
                emit(out,['--emit-split','--split-max-types',str(budget)],schema=schema)
                data=json.loads((out/'generated-sources.json').read_text())
                assert len(data['sources'])==count
                assert all(u['types']<=budget for u in data['units'] if u['file'] in data['sources'])
                calls=['#include "generated.h"','int main(void){ int sum=0;']
                for i in range(40):
                    n=f'stage3_mSolo_tType{i}'
                    calls.append(f'{{ {n} a,b; {n}_init(&a); {n}_init(&b); a.value={i%2}; '
                                 f'if({n}_copy(&a,&b)!=ASN1_OK)return 2;sum+=b.value;{n}_free(&a);{n}_free(&b); }}')
                calls.append('return sum!=20; }')
                driver=work/'solo.c';driver.write_text('\n'.join(calls))
                exe=work/'solo-run'
                run([a.cc,'-std=c11','-pedantic-errors','-Wall','-Wextra','-Werror',*san,
                     '-I',root/'include','-I',out,*[out/n for n in data['sources']],driver,a.runtime,'-o',exe])
                run([exe]);manifest(out)
            assert len(list(out.glob('*.c')))==4
        elif a.case=='rrc_types':
            out=work/'rrc'
            schema=root/'tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn'
            emit(out,['--split-max-types','128'],schema=schema)
            data=json.loads((out/'generated-sources.json').read_text())
            assert len(data['sources'])>1
            assert sum(u['types'] for u in data['units'] if u['file'] in data['sources'])>1000
            n='stage3_mEUTRA_x2dRRC_x2dDefinitions_tRRCConnectionRequest'
            driver=work/'rrc-main.c'
            driver.write_text('#include "generated.h"\nint main(void){'+n+' a,b;'+
                n+'_init(&a);'+n+'_init(&b);if('+n+'_reset_defaults(&a)!=ASN1_OK)return 1;'+
                'if('+n+'_copy(&a,&b)!=ASN1_OK)return 2;'+n+'_free(&a);'+n+'_free(&b);'+n+'_free(&b);return 0;}')
            exe=work/'rrc-run';rss=work/'rrc-rss.txt'
            command=[a.cc,'-std=c11','-pedantic-errors','-Wall','-Wextra','-Werror',*san,
                     '-I',root/'include','-I',out,*[out/name for name in data['sources']],driver,a.runtime,'-o',exe]
            if sys.platform.startswith('linux') and Path('/usr/bin/time').exists():
                command=['/usr/bin/time','-f','%M','-o',rss,*command]
            begin=time.perf_counter();run(command);elapsed=time.perf_counter()-begin
            run([exe]);manifest(out)
            # GCC UBSan itself depends on libstdc++; check released C binaries,
            # not the instrumentation toolchain's transitive libraries.
            if sys.platform.startswith('linux') and not a.sanitizers:
                deps=run(['ldd',exe]).stdout
                assert 'libstdc++' not in deps and 'libc++' not in deps
            metrics={'schema':'LTE RRC 8.6.0, types/lifecycle only','sources':len(data['sources']),
                'types':sum(u['types'] for u in data['units'] if u['file'] in data['sources']),
                'max_source_bytes':max(u['bytes'] for u in data['units'] if u['file'] in data['sources']),
                'max_source_functions':max(u['functions'] for u in data['units'] if u['file'] in data['sources']),
                'compile_link_seconds':round(elapsed,3),'peak_child_rss_kib':int(rss.read_text()) if rss.exists() else None}
            Path(a.work,'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
            print(json.dumps(metrics,indent=2))
        elif a.case=='publication':
            out=work/'publish';emit(out,['--emit-split','--split-max-types','2']);old=manifest(out)
            (out/'user.c').write_text('/* user */\n');(out/'user.h').write_text('/* user */\n')
            emit(out,['--split-max-types','1000']);new=manifest(out)
            assert len([n for n in new if n.endswith('.c')])==1
            assert all(not (out/n).exists() for n in old if n not in new)
            assert (out/'user.c').read_text()=='/* user */\n'
            stale=next(n for n in new if n.endswith('.c'));(out/stale).write_text('/* modified stale */\n')
            emit(out,['--emit-inline']);current=manifest(out)
            assert stale not in current and (out/stale).read_text()=='/* modified stale */\n'
            assert json.loads((out/'generated-sources.json').read_text())['sources']==[]
            # C -> C++ -> C: unchanged old owners disappear; edited/user files survive.
            small=work/'small.asn';small.write_text('Tiny DEFINITIONS ::= BEGIN Flag ::= BOOLEAN END\n')
            run([a.compiler,'--emit-dir',out,small]);cpp=manifest(out)
            assert all(not (out/n).exists() for n in current if n not in cpp)
            emit(out,schema=small);c=manifest(out)
            assert all(not (out/n).exists() for n in cpp if n not in c)
            assert (out/stale).read_text()=='/* modified stale */\n'
            # A failed model, lock, sidecar conflict or manifest never commits output.
            baseline=snapshot(out)
            bad=work/'bad.asn';bad.write_text('Bad DEFINITIONS ::= BEGIN X ::= SEQUENCE { n INTEGER DEFAULT missing } END\n')
            emit(out,schema=bad,success=False);assert snapshot(out)==baseline
            (out/'.asn1cxx-emitting').mkdir();locked=snapshot(out)
            emit(out,success=False);assert snapshot(out)==locked
            (out/'.asn1cxx-emitting').rmdir()
            (out/'.asn1cxx-manifest').write_text('broken\n');broken=snapshot(out)
            emit(out,success=False);assert snapshot(out)==broken
            collision=work/'collision';collision.mkdir();(collision/'generated-sources.json').write_text('user')
            emit(collision,success=False);assert snapshot(collision)=={'generated-sources.json':b'user'}
            if os.name!='nt':
                linked=work/'linked';linked.mkdir();target=work/'target';target.write_text('user')
                (linked/'generated.h').symlink_to(target)
                emit(linked,success=False);assert target.read_text()=='user' and (linked/'generated.h').is_symlink()
        elif a.case=='cache':
            out=work/'cached';emit(out,['--split-max-types','8']);initial=snapshot(out)
            owned=manifest(out);times={n:(out/n).stat().st_mtime_ns for n in owned}
            result=emit(out,['--split-max-types','8'])
            assert re.search(r'emit cache: (\d+)/\1 units hit',result.stderr),result.stderr
            assert snapshot(out)==initial and all((out/n).stat().st_mtime_ns==t for n,t in times.items())
            emit(out,['--no-emit-cache','--split-max-types','8']);assert manifest(out)==owned
            # Tamper a cache file and a current source: regenerate, never trust corruption.
            for f in (out/'.asn1cxx-cache').glob('*.cache'): f.write_text('corrupt')
            source=next(n for n in owned if n.endswith('.c'));(out/source).write_text('edited')
            emit(out,['--split-max-types','8']);assert manifest(out)==owned
            for flags in (['--split-max-types','4'],['--emit-split','--split-max-types','4'],['--emit-inline']):
                emit(out,flags);current=manifest(out)
                fresh=work/('fresh'+str(len(flags))+str(len(current)));emit(fresh,[*flags,'--no-emit-cache'])
                assert manifest(fresh)==current
            # Dependency content is part of the key even if rendered C stays equal.
            src=work/'dep.asn';src.write_text('D DEFINITIONS ::= BEGIN Flag ::= BOOLEAN END\n')
            dep=work/'dependency';emit(dep,schema=src)
            src.write_text(src.read_text()+'-- changed dependency\n')
            result=emit(dep,schema=src);assert 'emit cache: 0/' in result.stderr
        elif a.case=='options':
            small=work/'small.asn';small.write_text('T DEFINITIONS ::= BEGIN Flag ::= BOOLEAN END\n')
            for flags in (['--types-only'],['--split-max-types','2'],['--lang','c','--types-only','--codec','uper'],
                          ['--lang','c','--types-only','--emit-inline','--split-max-types','2'],
                          ['--lang','c','--types-only','--split-max-types','0'],
                          ['--lang','c','--types-only','--split-max-types','-1'],
                          ['--lang','c','--types-only','--split-max-types','999999999999999999999999999999'],
                          ['--lang','c','--types-only','--c-prefix','bad_']):
                out=work/'reject';run([a.compiler,*flags,'--emit-dir',out,small],False);assert not out.exists()
            out=work/'case';collision=work/'case.asn'
            collision.write_text('Mod DEFINITIONS ::= BEGIN First ::= BOOLEAN END\nMOD DEFINITIONS ::= BEGIN Second ::= BOOLEAN END\n')
            emit(out,['--emit-split'],schema=collision,success=False)
            assert not (out/'.asn1cxx-manifest').exists()
            # A cross-module recursive SCC exceeds a soft budget of one; keep it whole.
            result=emit(work/'scc',['--emit-split','--split-max-types','1'])
            assert 'SCC exceeds split-max-types' in result.stderr
            data=json.loads((work/'scc/generated-sources.json').read_text())
            assert any(u['types']>1 for u in data['units'] if u['file'] in data['sources'])
        elif a.case=='cmake':
            out=work/'generated';emit(out,['--emit-split','--split-max-types','8'])
            project=work/'consumer';project.mkdir()
            (project/'main.c').write_text('#include "generated.h"\nint second(void);int main(void){'
                'stage3_mA_tDefaults v;stage3_mA_tDefaults_init(&v);return v.fnumber.value!=7 || second();}')
            (project/'second.c').write_text('#include "generated.h"\nint second(void){stage3_mA_tFlag v;'
                                          'stage3_mA_tFlag_init(&v);return v.value;}')
            sanitizer='target_compile_options(consumer PRIVATE '+ ' '.join(san)+')\ntarget_link_options(consumer PRIVATE -fsanitize=address,undefined)\n' if san else ''
            (project/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.16)\nproject(COnly LANGUAGES C)\n'
                f'include("{out.as_posix()}/generated-sources.cmake")\n'
                'add_executable(consumer main.c second.c ${ASN1_GENERATED_SOURCES})\n'
                'set_target_properties(consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)\n'
                f'target_include_directories(consumer PRIVATE "{out.as_posix()}" "{root.as_posix()}/include")\n'
                f'target_link_libraries(consumer PRIVATE "{Path(a.runtime).as_posix()}")\n'+sanitizer+
                'enable_testing()\nadd_test(NAME CallsLifecycle COMMAND consumer)\n')
            run([a.cmake,'-S',project,'-B',work/'build','-DCMAKE_C_COMPILER='+a.cc])
            run([a.cmake,'--build',work/'build','--config','Release'])
            ctest=str(Path(a.cmake).with_name('ctest.exe' if os.name=='nt' else 'ctest'))
            run([ctest,'--test-dir',work/'build','-C','Release','--output-on-failure'])
        else: raise AssertionError(a.case)
    print('PASS CGenBatch3 '+a.case)


if __name__=='__main__':
    main()
