"""Configure-time C source planning: schema growth, missing outputs, cross build."""
import argparse
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
for key in ('cmake','compiler','cc','runtime','root','work'):
    p.add_argument('--'+key,required=True)
p.add_argument('--sanitizers',action='store_true')
a=p.parse_args()
root=Path(a.root).resolve(); work=Path(a.work).resolve()
work.mkdir(parents=True,exist_ok=True)
project=work/'project';project.mkdir(exist_ok=True)
schema=project/'input.asn'
schema.write_text('M DEFINITIONS ::= BEGIN Record ::= SEQUENCE { count INTEGER (0..255) } END\n')
for name in ('generated_c_main.c','generated_c_second.c'):
    (project/name).write_bytes((root/'examples/package-consumers'/name).read_bytes())
san=('target_compile_options(consumer PRIVATE -fsanitize=address,undefined)\n'
     'target_link_options(consumer PRIVATE -fsanitize=address,undefined)\n') if a.sanitizers else ''
(project/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.16)
project(PlanConsumer LANGUAGES C)
include("{root.as_posix()}/cmake/GenerateC.cmake")
asn1_generate_c(sources GENERATOR "{Path(a.compiler).as_posix()}"
 OUTPUT_DIRECTORY "${{CMAKE_CURRENT_BINARY_DIR}}/gen" CODECS uper,ber
 SCHEMAS input.asn OPTIONS --emit-split --split-max-types 1)
add_executable(consumer generated_c_main.c generated_c_second.c ${{sources}})
target_include_directories(consumer PRIVATE "${{sources_INCLUDE_DIRECTORY}}" "{root.as_posix()}/include")
target_link_libraries(consumer PRIVATE "{Path(a.runtime).as_posix()}")
set_target_properties(consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(NOT MSVC)
 target_link_libraries(consumer PRIVATE m)
endif()
{san}
if(NOT CMAKE_CROSSCOMPILING)
 enable_testing()
 add_test(NAME CallsBothTUs COMMAND consumer)
endif()
''')

def run(command):
    result=subprocess.run(list(map(str,command)),capture_output=True,text=True)
    if result.returncode:
        raise RuntimeError(str(command)+'\n'+result.stdout+result.stderr)
    return result.stdout

build=work/'native'
run([a.cmake,'-S',project,'-B',build,'-DCMAKE_C_COMPILER='+a.cc])
def compile_and_run():
    run([a.cmake,'--build',build,'--config','Release','--parallel','2'])
    ctest=str(Path(a.cmake).with_name('ctest.exe' if Path(a.cmake).suffix=='.exe' else 'ctest'))
    run([ctest,'--test-dir',build,'-C','Release','--output-on-failure'])
compile_and_run()
manifest=build/'gen/generated-sources.json'
before=json.loads(manifest.read_text())['sources']
schema.write_text(schema.read_text()+'N DEFINITIONS ::= BEGIN Added ::= BOOLEAN END\n')
compile_and_run() # no explicit configure: CMAKE_CONFIGURE_DEPENDS must update sources
after=json.loads(manifest.read_text())['sources']
assert set(after)-set(before), (before,after)
missing=build/'gen'/after[-1]
missing.unlink()
compile_and_run()
assert missing.exists()
if Path(a.cc).name.lower() not in ('cl.exe','cl','clang-cl','clang-cl.exe'):
    toolchain=work/'cross.cmake'
    toolchain.write_text('set(CMAKE_SYSTEM_NAME Generic)\nset(CMAKE_C_COMPILER "'+Path(a.cc).as_posix()+'")\n')
    cross=work/'cross'
    run([a.cmake,'-S',project,'-B',cross,'-DCMAKE_TOOLCHAIN_FILE='+str(toolchain)])
    run([a.cmake,'--build',cross,'--parallel','2'])
    # Generic is a build-system execution guard probe, not a target ABI claim.
    assert not (cross/'CTestTestfile.cmake').exists()
print('PASS C-only multi-TU planning, new module, missing source repair and applicable cross execution guard')
