"""Exercise local commits and refusal paths in disposable Git repositories; no network or Codex."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('nightly', ROOT/'scripts/nightly_c_codegen.py')
nightly = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nightly)


class CommitSafety(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='asn1-nightly-test-')
        self.repo = Path(self.temp.name)
        self.project = self.repo/'project'
        self.project.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Runner Test')
        self.git('config', 'user.email', 'runner-test@example.invalid')
        self.git('config', 'commit.gpgSign', 'false')
        (self.project/'seed.txt').write_text('baseline\n')
        (self.repo/'outside.txt').write_text('baseline\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'baseline')
        self.guard = nightly.GitGuard(self.project)
        self.head = self.guard.git('rev-parse','HEAD')
        self.note = self.repo/'message.txt'  # created only after the path snapshot

    def tearDown(self):
        self.temp.cleanup()

    def git(self,*args):
        return subprocess.run(['git',*args],cwd=self.repo,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

    def commit(self, verify=lambda: None, checkpoint=lambda tree: None):
        return self.guard.commit_batch(1,'test-run',self.note,checkpoint,verify)

    def test_success_commits_only_project_and_never_uses_a_remote(self):
        (self.project/'seed.txt').write_text('implemented\n')
        checkpoint=[]
        head=self.commit(checkpoint=checkpoint.append)
        self.assertNotEqual(head,self.head)
        self.assertEqual(len(checkpoint),1)
        self.assertEqual(self.guard.git('log','-1','--format=%T'),checkpoint[0])
        self.assertIn('CGen-Batch: 1',self.guard.git('log','-1','--format=%B'))
        self.assertEqual(self.guard.git('remote'),'')
        self.assertEqual(self.guard.git('diff','--cached','--name-only'),'')

    def test_failed_gate_does_not_stage_or_commit(self):
        (self.project/'seed.txt').write_text('unfinished\n')
        def rejected():
            raise RuntimeError('CTest failed')
        with self.assertRaisesRegex(RuntimeError,'CTest failed'):
            self.commit(verify=rejected)
        self.assertEqual(self.guard.git('rev-parse','HEAD'),self.head)
        self.assertEqual(self.guard.git('diff','--cached','--name-only'),'')
        self.assertEqual((self.project/'seed.txt').read_text(),'unfinished\n')

    def test_changes_in_parent_project_are_never_staged(self):
        (self.repo/'outside.txt').write_text('user work\n')
        (self.project/'seed.txt').write_text('implementation\n')
        with self.assertRaisesRegex(RuntimeError,'outside project'):
            self.commit()
        self.assertEqual(self.guard.git('rev-parse','HEAD'),self.head)
        self.assertEqual(self.guard.git('diff','--cached','--name-only'),'')

    def test_deletion_and_unicode_filename_are_staged_exactly(self):
        (self.project/'seed.txt').unlink()
        (self.project/'类型 with spaces.txt').write_text('new\n')
        head=self.commit()
        self.assertNotEqual(head,self.head)
        self.assertFalse((self.project/'seed.txt').exists())
        self.assertEqual(self.guard.git('status','--porcelain','--','project'),'')

    def test_protected_runner_source_is_not_committed(self):
        path=self.project/'scripts/nightly_c_codegen.py'
        path.parent.mkdir()
        path.write_text('changed')
        with self.assertRaisesRegex(RuntimeError,'Runner/gate source'):
            self.commit()
        self.assertEqual(self.guard.git('rev-parse','HEAD'),self.head)

    def test_checkpoint_failure_preserves_head_and_index_for_recovery(self):
        (self.project/'seed.txt').write_text('ready\n')
        def fail_checkpoint(tree):
            raise OSError('state storage unavailable')
        with self.assertRaises(OSError):
            self.commit(checkpoint=fail_checkpoint)
        self.assertEqual(self.guard.git('rev-parse','HEAD'),self.head)
        self.assertEqual(self.guard.git('diff','--cached','--name-only'),'project/seed.txt')

    def test_interrupted_state_save_recovers_exact_commit_without_duplicate(self):
        (self.project/'seed.txt').write_text('implemented\n')
        state={'head':self.head,'phase':'committing','batch':1,'run_id':'test-run',
               'completed':[],'verified_tests':['old.test','CGenBatch1.Cli']}
        head=self.commit(checkpoint=lambda tree: state.update(tree=tree))
        self.assertTrue(nightly.recover_commit(self.guard,state))
        self.assertEqual(state['head'],head)
        self.assertEqual(state['completed'],[1])
        self.assertEqual(state['baseline_tests'],['old.test','CGenBatch1.Cli'])
        self.assertFalse(nightly.recover_commit(self.guard,state))

    def test_head_and_branch_drift_are_detected(self):
        branch=self.guard.git('branch','--show-current')
        self.git('commit','--allow-empty','-qm','external change')
        with self.assertRaisesRegex(RuntimeError,'Branch/HEAD changed'):
            self.guard.ensure_identity(branch,self.head)


class PlanAndLockSafety(unittest.TestCase):
    def test_later_tasks_and_acceptance_text_remain_distinguishable(self):
        plan=(ROOT/'docs/c-codegen-todo.md').read_text(encoding='utf-8')
        changed=plan.replace('- [ ] **CGEN-01','- [x] **CGEN-01',1)
        self.assertFalse(nightly.task_flags(plan)[1])
        self.assertTrue(nightly.task_flags(changed)[1])
        self.assertEqual(nightly.immutable_plan(plan),nightly.immutable_plan(changed))
        self.assertNotEqual(nightly.immutable_plan(plan),nightly.immutable_plan(plan.replace('CGEN-01','CGEN-99',1)))
        with self.assertRaises(RuntimeError):
            nightly.task_flags(plan.replace('CGEN-01','CGEN-99',1))

    def test_second_runner_never_reclaims_existing_lock(self):
        with tempfile.TemporaryDirectory(prefix='asn1-nightly-lock-') as folder:
            base=Path(folder)
            with nightly.single_runner(base):
                with self.assertRaises(FileExistsError):
                    with nightly.single_runner(base):
                        self.fail('second runner acquired the lock')
                self.assertTrue((base/'runner.lock').exists())
            self.assertFalse((base/'runner.lock').exists())


if __name__=='__main__':
    unittest.main(verbosity=2)
