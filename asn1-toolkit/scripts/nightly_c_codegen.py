"""Serial C-codegen TODO runner: independently gate each batch, commit locally, never push."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import time
import uuid

PROJECT = Path(__file__).resolve().parents[1]
BATCHES = {1: (1, 3), 2: (4, 6), 3: (7, 9), 4: (10, 12),
           5: (13, 15), 6: (16, 19), 7: (20, 24)}
PROTECTED = ('scripts/nightly_c_codegen.py', 'scripts/run-c-codegen-nightly.ps1',
             'tests/scripts/test_nightly_c_codegen.py', 'scripts/validate_codec_matrix.py')
TASK_PATTERN = re.compile(r'^- \[([ xX])\] \*\*CGEN-(\d{2})', re.M)


def command(argv, cwd=PROJECT, **kwargs):
    return subprocess.run([str(x) for x in argv], cwd=cwd, check=True, **kwargs)


def output(argv, cwd=PROJECT):
    return command(argv, cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.decode('utf-8').strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def task_flags(text):
    matches = TASK_PATTERN.findall(text)
    if [int(number) for _, number in matches] != list(range(1, 25)):
        raise RuntimeError('TODO must retain exactly CGEN-01..24 in order')
    return {int(number): mark.lower() == 'x' for mark, number in matches}


def immutable_plan(text):
    # Permit progress summary edits before the first task, but not rewritten acceptance criteria.
    start = TASK_PATTERN.search(text)
    if start is None:
        raise RuntimeError('Missing CGEN tasks')
    return TASK_PATTERN.sub(lambda m: '- [ ] **CGEN-' + m.group(2), text[start.start():])


class GitGuard:
    def __init__(self, project):
        self.project = Path(project).resolve()
        self.repo = Path(output(['git', 'rev-parse', '--show-toplevel'], self.project)).resolve()
        self.prefix = self.project.relative_to(self.repo).as_posix()
        if self.prefix == '.':
            self.prefix = ''

    def git(self, *args):
        return output(['git', *args], self.repo)

    def changed(self):
        args = (['diff', '--name-only', '--no-renames', '-z', 'HEAD'],
                ['ls-files', '--others', '--exclude-standard', '-z'])
        paths = set()
        for argv in args:
            raw = command(['git', *argv], self.repo, stdout=subprocess.PIPE).stdout
            paths.update(part.decode('utf-8') for part in raw.split(b'\0') if part)
        return sorted(paths)

    def local_name(self, path):
        prefix = self.prefix + '/' if self.prefix else ''
        if prefix and not path.startswith(prefix):
            raise RuntimeError('Changes outside project: ' + path)
        return path[len(prefix):]

    def validate_scope(self, paths):
        for path in paths:
            local = self.local_name(path)
            if local in PROTECTED:
                raise RuntimeError('Runner/gate source may not change during unattended execution: ' + path)
        return paths

    def ensure_identity(self, branch, head):
        if self.git('branch', '--show-current') != branch or self.git('rev-parse', 'HEAD') != head:
            raise RuntimeError('Branch/HEAD changed outside the runner; preserving working tree')

    def commit_batch(self, batch, run_id, message_file, checkpoint, verify):
        verify()  # Acceptance failure must happen before staging or committing.
        paths = self.validate_scope(self.changed())
        if not paths:
            raise RuntimeError('No changes: cannot manufacture a completed batch commit')
        command(['git', '--literal-pathspecs', 'add', '-A', '--', *paths], self.repo)
        raw = command(['git','diff','--cached','--name-only','--no-renames','-z'], self.repo,
                      stdout=subprocess.PIPE).stdout
        staged = [part.decode('utf-8') for part in raw.split(b'\0') if part]
        self.validate_scope(staged)
        if set(staged) != set(paths):
            raise RuntimeError('Staged paths differ from reviewed batch paths')
        tree = self.git('write-tree')
        message_file.write_text(f'feat(cgen): complete batch {batch}\n\n'
                                f'CGen-Batch: {batch}\nCGen-Run: {run_id}\n', encoding='utf-8')
        checkpoint(tree)
        command(['git', 'commit', '-F', message_file], self.repo)
        return self.git('rev-parse', 'HEAD')



def recover_commit(guard, state):
    """Reconcile a local commit with its write-ahead state after an interrupted state save."""
    head = guard.git('rev-parse','HEAD')
    if state.get('phase') != 'committing' or head == state['head']:
        return False
    message = guard.git('log','-1','--format=%B')
    if (guard.git('rev-parse','HEAD^') != state['head'] or
        guard.git('rev-parse','HEAD^{tree}') != state['tree'] or
        f"CGen-Run: {state['run_id']}" not in message or
        f"CGen-Batch: {state['batch']}" not in message):
        raise RuntimeError('Unexpected HEAD during interrupted commit recovery')
    state['completed'].append(state['batch'])
    state.update(head=head, phase='ready',baseline_tests=state.pop('verified_tests'))
    return True


def atomic_json(path, value):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temp.replace(path)


@contextmanager
def single_runner(folder):
    lock = folder / 'runner.lock'
    # Exclusive creation; never reclaim another process's lock automatically.
    with lock.open('x', encoding='utf-8') as stream:
        stream.write(str(os.getpid()))
    try:
        yield
    finally:
        lock.unlink(missing_ok=True)


def find_codex(explicit):
    if explicit:
        path = Path(explicit).resolve()
    elif shutil.which('codex'):
        path = Path(shutil.which('codex'))
    else:
        base = Path(os.environ.get('LOCALAPPDATA', '')) / 'OpenAI/Codex/bin'
        candidates = list(base.glob('*/codex.exe'))
        if not candidates:
            raise RuntimeError('Codex executable not found; supply --codex PATH')
        path = max(candidates, key=lambda p: p.stat().st_mtime)
    if not path.is_file():
        raise RuntimeError('Codex executable does not exist: ' + str(path))
    return path


def preflight(codex, guard):
    if os.name != 'nt':
        raise RuntimeError('Launch this runner from normal Windows PowerShell (WSL is used for gates)')
    for name in ('git', 'wsl', 'python'):
        if not shutil.which(name):
            raise RuntimeError('Missing tool: ' + name)
    auth = subprocess.run([str(codex), 'login', 'status'], cwd=PROJECT,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if auth.returncode or 'not logged in' in auth.stdout.lower():
        raise RuntimeError('CLI login unavailable in this environment; run the launcher in your logged-in terminal')
    help_text = output([codex, 'exec', '--help'])
    for option in ('--approve-for-me', '--output-schema', '--json'):
        if option not in help_text:
            raise RuntimeError('Unsupported CLI option: ' + option)
    branch = guard.git('branch', '--show-current')
    if not branch:
        raise RuntimeError('Detached HEAD is not supported')
    for key in ('user.name', 'user.email'):
        if not guard.git('config', key):
            raise RuntimeError('Missing git identity: ' + key)
    linux = output(['wsl', '--exec', 'wslpath', '-a', str(PROJECT)])
    tools = 'set -eu; command -v gcc; command -v g++; command -v cmake; command -v ctest; command -v ninja; command -v python3; command -v asn1c'
    command(['wsl', '--exec', 'sh', '-lc', tools], stdout=subprocess.DEVNULL)
    if not (PROJECT / 'build/_deps/googletest-src/CMakeLists.txt').is_file():
        raise RuntimeError('Offline GoogleTest source missing: build/_deps/googletest-src')
    print(f'Preflight OK. Branch={branch}; Codex={codex}; project={PROJECT}', flush=True)
    print('No push commands. Working tree must be clean before a new run.', flush=True)
    return branch, linux


def wsl_logged(linux, script, log):
    with log.open('w', encoding='utf-8') as stream:
        command(['wsl', '--exec', 'sh', '-lc', 'cd ' + shlex.quote(linux) + ' && ' + script],
                stdout=stream, stderr=subprocess.STDOUT)


def test_names(linux, build, log):
    wsl_logged(linux, 'ctest --test-dir ' + build + ' --show-only=json-v1', log)
    data = json.loads(log.read_text(encoding='utf-8'))
    return set(test['name'] for test in data['tests'])


def gates(linux, batch, folder, baseline):
    source = shlex.quote(linux + '/build/_deps/googletest-src')
    names = None
    for mode, kind, sanitizer in (('release', 'Release', 'OFF'), ('san', 'Debug', 'ON')):
        build = 'build-rev1-' + mode
        flags = (' -DASN1_BUILD_TESTS=ON -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=ON'
                 ' -DASN1_BUILD_C_RUNTIME=ON -DCMAKE_BUILD_TYPE=' + kind +
                 ' -DASN1_ENABLE_SANITIZERS=' + sanitizer + ' -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=' + source)
        wsl_logged(linux, 'cmake -S . -B ' + build + flags + ' && cmake --build ' + build + ' -j1',
                   folder / (mode + '-build.log'))
        current = test_names(linux, build, folder / (mode + '-tests.json'))
        if not baseline.issubset(current):
            raise RuntimeError('Pre-existing tests disappeared: ' + ', '.join(sorted(baseline-current)))
        if not any(name.startswith(f'CGenBatch{batch}.') for name in current):
            raise RuntimeError(f'No meaningful stage acceptance tests named CGenBatch{batch}.* registered')
        env = ('ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 '
               if mode == 'san' else '')
        wsl_logged(linux, env + 'ctest --test-dir ' + build + ' --output-on-failure --timeout 600',
                   folder / (mode + '-test.log'))
        names = current
    wsl_logged(linux, 'python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx'
               ' --cxx g++ --output build-rev-cgen-nightly/matrix', folder / 'matrix.log')
    wsl_logged(linux, 'python3 scripts/validate_layering.py --cmake cmake', folder / 'layering.log')
    return names


def agent_prompt(batch, attempt, previous):
    first, last = BATCHES[batch]
    return f'''Execute ONLY batch {batch} (CGEN-{first:02d}..CGEN-{last:02d}) of docs/c-codegen-todo.md.
Read the project skill docs/skills/asn1-remediation/SKILL.md and applicable AGENTS.md.
Implement necessary minimal dependencies; do not start later batches or rewrite acceptance criteria.
Preserve compiler/runtime separation, Result/Diagnostics, pure C11, shared PER Variant, and existing C++ behaviour.
You may edit files only inside this project. NEVER commit, push, change branch/HEAD, reset, clean, stash,
remove baseline tests, or weaken checks. The parent runner controls commits. Never read or print credential files. Do not touch the runner,
its launcher/tests, or scripts/validate_codec_matrix.py. Do not execute another Codex or delegate work.
Register real, stage-specific CTest acceptance tests named CGenBatch{batch}.* (not dummy tests or wording checks).
Run stage checks and update corresponding docs and the project skill. Mark only this batch's CGEN checkboxes
when requirements are fulfilled; leave later batches unchanged. The TODO acceptance text must remain unchanged
apart from checkboxes; progress summaries before the first task may be updated. Put evidence in a separate doc.
The parent independently runs Release, ASan+UBSan, all existing tests, layering and the 84-entry C++ matrix.
After edits, return JSON with status=complete only if every assigned task is satisfied; otherwise blocked
with concrete remaining work. No successful stubs and no pretending unsupported C runtime APIs exist.
Attempt {attempt} of 3. Previous attempt/gate result (if any):
{previous[-12000:]}
'''


def run_agent(codex, batch, attempt, previous, folder):
    schema = {'type': 'object', 'properties': {
        'status': {'type': 'string', 'enum': ['complete', 'blocked']},
        'summary': {'type': 'string'}, 'remaining': {'type': 'array', 'items': {'type': 'string'}}},
        'required': ['status', 'summary', 'remaining'], 'additionalProperties': False}
    atomic_json(folder/'result-schema.json', schema)
    prompt = agent_prompt(batch, attempt, previous)
    (folder/'prompt.txt').write_text(prompt, encoding='utf-8')
    with (folder/'agent.jsonl').open('w', encoding='utf-8') as events, (folder/'agent.stderr.log').open('w', encoding='utf-8') as errors:
        process = subprocess.run([str(codex), 'exec', '--approve-for-me', '--json', '-C', str(PROJECT),
                                  '--output-schema', str(folder/'result-schema.json'),
                                  '-o', str(folder/'result.json'), '-'], input=prompt.encode('utf-8'),
                                 cwd=PROJECT, stdout=events, stderr=errors)
    if process.returncode:
        raise RuntimeError('Codex exited with error; see ' + str(folder/'agent.stderr.log'))
    result = json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
    if result.get('status') != 'complete' or result.get('remaining'):
        raise RuntimeError('Agent reports incomplete: ' + json.dumps(result, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', help='Execute; default is preflight only')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--through-batch', type=int, choices=range(1,8), default=7)
    parser.add_argument('--max-hours', type=float, default=10, help='Do not start another attempt/batch after this budget; does not kill an active command')
    parser.add_argument('--codex')
    args = parser.parse_args()
    if args.max_hours <= 0:
        parser.error('--max-hours must be positive')
    codex = find_codex(args.codex)
    guard = GitGuard(PROJECT)
    branch, linux = preflight(codex, guard)
    changed = guard.changed()
    if not args.run:
        print('Pending files:', len(changed))
        for path in changed:
            print('  ' + path)
        if changed:
            raise RuntimeError('Preflight cannot start a new run until existing changes have their own commit')
        return
    base = PROJECT/'build-rev-cgen-nightly'
    base.mkdir(exist_ok=True)
    state_file = base/'state.json'
    deadline = time.monotonic() + args.max_hours*3600
    with single_runner(base):
        plan_text = (PROJECT/'docs/c-codegen-todo.md').read_text(encoding='utf-8')
        if args.resume:
            state = json.loads(state_file.read_text(encoding='utf-8'))
            if state['branch'] != branch:
                raise RuntimeError('Resume requires original branch')
            if recover_commit(guard, state):
                atomic_json(state_file,state)
        else:
            if state_file.exists():
                raise RuntimeError('State already exists; use --resume. Do not erase another run state')
            if changed:
                raise RuntimeError('Commit existing work separately before starting')
            if any(task_flags(plan_text).values()):
                raise RuntimeError('New run expects all CGEN tasks pending; use a reviewed resume state')
            baseline = test_names(linux,'build-rev1-release',base/'baseline-tests.json')
            if len(baseline) < 442:
                raise RuntimeError('Baseline build does not expose the expected existing tests')
            state = {'run_id':uuid.uuid4().hex,'branch':branch,'head':guard.git('rev-parse','HEAD'),
                     'completed':[], 'phase':'ready', 'baseline_tests':sorted(baseline),
                     'plan':immutable_plan(plan_text),
                     'protected':{name:digest(PROJECT/name) for name in PROTECTED}}
            atomic_json(state_file,state)
        try:
            for batch in range(1,args.through_batch+1):
                if batch in state['completed']:
                    continue
                previous = state.get('error','')
                success = False
                for attempt in range(1,4):
                    if time.monotonic() >= deadline:
                        state.update(phase='budget-stop',batch=batch)
                        atomic_json(state_file,state)
                        print('Budget reached at a boundary; no new batch/attempt started.',flush=True)
                        return
                    guard.ensure_identity(branch,state['head'])
                    guard.validate_scope(guard.changed())
                    for name, expected in state['protected'].items():
                        if digest(PROJECT/name) != expected:
                            raise RuntimeError('Protected runner/gate changed: '+name)
                    folder = base/f'batch-{batch}'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')+f'-attempt-{attempt}')
                    folder.mkdir(parents=True)
                    state.update(phase='implementing', batch=batch, attempt=attempt, log=str(folder))
                    atomic_json(state_file,state)
                    print(f'Batch {batch}, attempt {attempt}: {folder}',flush=True)
                    try:
                        run_agent(codex,batch,attempt,previous,folder)
                        guard.ensure_identity(branch,state['head'])
                        guard.validate_scope(guard.changed())
                        for name, expected in state['protected'].items():
                            if digest(PROJECT/name) != expected:
                                raise RuntimeError('Protected runner/gate changed: '+name)
                        text=(PROJECT/'docs/c-codegen-todo.md').read_text(encoding='utf-8')
                        if immutable_plan(text) != state['plan']:
                            raise RuntimeError('TODO acceptance criteria were modified')
                        flags=task_flags(text)
                        done={n for b in state['completed']+[batch] for n in range(BATCHES[b][0],BATCHES[b][1]+1)}
                        if any(flags[n] != (n in done) for n in range(1,25)):
                            raise RuntimeError('Task checkboxes do not match this batch and completed predecessors')
                        state['phase']='testing';atomic_json(state_file,state)
                        def verify():
                            names = gates(linux,batch,folder,set(state['baseline_tests']))
                            guard.ensure_identity(branch,state['head'])
                            state['verified_tests'] = sorted(names)
                        def before_commit(tree):
                            state.update(phase='committing',tree=tree)
                            atomic_json(state_file,state)
                        head=guard.commit_batch(batch,state['run_id'],folder/'commit-message.txt',before_commit,verify)
                        state['completed'].append(batch)
                        state.update(head=head,phase='ready',error='',baseline_tests=state.pop('verified_tests'))
                        atomic_json(state_file,state)
                        print(f'Committed batch {batch}: {head}; NOT pushed',flush=True)
                        success=True
                        break
                    except (RuntimeError, subprocess.CalledProcessError, ValueError) as error:
                        if state.get('phase') == 'committing':
                            # Keep the write-ahead checkpoint; never rerun the agent after a possibly successful commit.
                            raise
                        previous=f'{error}\nInspect logs in {folder}; repair the cause without weakening acceptance.'
                        state.update(phase='retry',error=previous)
                        atomic_json(state_file,state)
                        print(previous,flush=True)
                if not success:
                    raise RuntimeError(f'Batch {batch} failed after 3 attempts; no commit for this batch')
            print('Requested batches complete. Local commits only; no push performed.',flush=True)
        except BaseException as error:
            if state.get('phase') != 'committing':
                state.update(phase='stopped',error=str(error))
            atomic_json(state_file,state)
            raise


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError, ValueError) as error:
        print('STOP: '+str(error),file=sys.stderr)
        sys.exit(1)
