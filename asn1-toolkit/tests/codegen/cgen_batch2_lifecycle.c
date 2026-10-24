#include "generated.h"
#include "cgen_batch2_allocator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static size_t live, calls, fail_at;
void *batch2_malloc(size_t size) {
  void *p;
  if (++calls == fail_at) return NULL;
  p = malloc(size);
  if (p) ++live;
  return p;
}
void *batch2_calloc(size_t count, size_t size) {
  void *p;
  if (++calls == fail_at) return NULL;
  p = calloc(count,size);
  if (p) ++live;
  return p;
}
void batch2_free(void *p) { if (p) { --live; free(p); } }
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int batch2_second(void);
static void bytes(stage2_mA_tBytes *v, unsigned char c) {
  v->data = batch2_malloc(1);
  if (!v->data) abort();
  v->data[0] = c; v->count = v->capacity = 1;
}
static int size_membership(void) {
  stage2_mA_tSizedBytes src, out;
  stage2_mA_tSizedBag bag, bag_out;
  stage2_mA_tSizedSet set, set_out;
  stage2_mA_tEmptyBytes empty, empty_out;
  stage2_mA_tSizedBytes_init(&src); stage2_mA_tSizedBytes_init(&out);
  stage2_mA_tSizedBag_init(&bag); stage2_mA_tSizedBag_init(&bag_out);
  stage2_mA_tSizedSet_init(&set); stage2_mA_tSizedSet_init(&set_out);
  stage2_mA_tEmptyBytes_init(&empty); stage2_mA_tEmptyBytes_init(&empty_out);
  src.data = batch2_calloc(9,1); src.capacity = 9;
  bag.data = batch2_calloc(9,sizeof(*bag.data)); bag.capacity = 9;
  set.data = batch2_calloc(9,sizeof(*set.data)); set.capacity = 9;
  CHECK(src.data && bag.data && set.data);
  for (size_t i = 0; i <= 9; ++i) {
    asn1_status_t expected = i <= 2 || i >= 7 ? ASN1_OK : ASN1_ERR_CONSTRAINT_VIOLATION;
    size_t old_count = out.count, old_bag = bag_out.count, old_set = set_out.count;
    src.count = bag.count = set.count = i;
    CHECK(stage2_mA_tSizedBytes_copy(&src,&out) == expected);
    CHECK(stage2_mA_tSizedBag_copy(&bag,&bag_out) == expected);
    CHECK(stage2_mA_tSizedSet_copy(&set,&set_out) == expected);
    if (expected != ASN1_OK) {
      CHECK(out.count == old_count && bag_out.count == old_bag && set_out.count == old_set);
    }
  }
  CHECK(out.data != src.data && bag_out.data != bag.data && set_out.data != set.data);
  {
    size_t baseline = live;
    unsigned char *old = out.data;
    calls = 0; fail_at = 1;
    CHECK(stage2_mA_tSizedBytes_copy(&src,&out) == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(out.data == old && live == baseline);
    calls = 0;
    CHECK(stage2_mA_tSizedBag_copy(&bag,&bag_out) == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(live == baseline && bag_out.count == 9);
    fail_at = 0;
  }
  CHECK(stage2_mA_tEmptyBytes_copy(&empty,&empty_out) == ASN1_OK);
  empty.data = batch2_malloc(1); CHECK(empty.data); empty.count = empty.capacity = 1;
  CHECK(stage2_mA_tEmptyBytes_copy(&empty,&empty_out) == ASN1_ERR_CONSTRAINT_VIOLATION);
  CHECK(empty_out.data == NULL && empty_out.count == 0);
  stage2_mA_tSizedBytes_free(&src); stage2_mA_tSizedBytes_free(&out);
  stage2_mA_tSizedBag_free(&bag); stage2_mA_tSizedBag_free(&bag_out);
  stage2_mA_tSizedSet_free(&set); stage2_mA_tSizedSet_free(&set_out);
  stage2_mA_tEmptyBytes_free(&empty); stage2_mA_tEmptyBytes_free(&empty_out);
  CHECK(live == 0);
  return 0;
}

static int profiles(void) {
  stage2_mA_tOwningDefaults value, copy;
  stage2_mA_tBoundedHuge big, big_out;
  stage2_mA_tBits bits, bits_out;
  stage2_mA_tMode mode, mode_out;
  stage2_mA_tIdentifier oid, oid_out;
  stage2_mA_tRelative rel, rel_out;
  stage2_mA_tReading real, real_out;
  stage2_mA_tPayload raw, raw_out;
  stage2_mA_tFixed fixed, fixed_out;
  stage2_mA_tOwningDefaults_init(&value); stage2_mA_tOwningDefaults_init(&copy);
  CHECK(value.ftext.data==NULL && value.fnumber.value.limbs==NULL);
  value.ftext.data=batch2_malloc(1); CHECK(value.ftext.data);
  value.ftext.data[0]='q'; value.ftext.count=value.ftext.capacity=1;
  {
    size_t failures=0;
    for(size_t allocation=1;allocation<10;++allocation) {
      size_t before=live; uint8_t *old=value.ftext.data;
      calls=0; fail_at=allocation;
      asn1_status_t status=stage2_mA_tOwningDefaults_reset_defaults(&value);
      if(status==ASN1_OK) break;
      CHECK(status==ASN1_ERR_OUT_OF_MEMORY && live==before && value.ftext.data==old);
      CHECK(value.ftext.data[0]=='q'); ++failures;
    }
    CHECK(failures==4); fail_at=0;
  }
  CHECK(value.fbits.value.bit_length==5 && value.fbits.value.bits[0]==0xa8);
  CHECK(value.foctets.count==2 && value.foctets.data[0]==0xab && value.foctets.data[1]==0xcd);
  CHECK(value.ftext.count==4 && memcmp(value.ftext.data,"demo",4)==0);
  CHECK(value.fnumber.value.sign==1 && value.fnumber.value.nlimbs==5 && value.fnumber.value.limbs[4]==1);
  CHECK(stage2_mA_tOwningDefaults_copy(&value,&copy)==ASN1_OK);
  CHECK(value.ftext.data!=copy.ftext.data && value.fnumber.value.limbs!=copy.fnumber.value.limbs);
  CHECK(stage2_mA_tOwningDefaults_copy(&copy,&copy)==ASN1_OK);
  CHECK(stage2_mA_tOwningDefaults_reset_defaults(&copy)==ASN1_OK);
  stage2_mA_tOwningDefaults_free(&value); stage2_mA_tOwningDefaults_free(&value);
  stage2_mA_tOwningDefaults_free(&copy);
  CHECK(live==0);
  {
    stage2_mA_tNestedOwning nested;
    stage2_mA_tEnumDefault enumeration;
    stage2_mA_tNestedOwning_init(&nested);
    CHECK(stage2_mA_tNestedOwning_reset_defaults(&nested)==ASN1_OK);
    CHECK(nested.frequired.ftext.count==4 && !nested.has_foptional && nested.foptional.ftext.data==NULL);
    stage2_mA_tNestedOwning_free(&nested);
    stage2_mA_tEnumDefault_init(&enumeration);
    CHECK(enumeration.fmode.value==9);
    stage2_mA_tEnumDefault_free(&enumeration);
  }
  stage2_mA_tBoundedHuge_init(&big); stage2_mA_tBoundedHuge_init(&big_out);
  big.value.limbs=batch2_calloc(5,sizeof(uint32_t)); CHECK(big.value.limbs);
  big.value.sign=1; big.value.nlimbs=big.value.cap=5; big.value.limbs[4]=1;
  CHECK(stage2_mA_tBoundedHuge_copy(&big,&big_out)==ASN1_OK);
  CHECK(big_out.value.limbs!=big.value.limbs);
  big.value.sign=-1; CHECK(stage2_mA_tBoundedHuge_copy(&big,&big_out)==ASN1_OK);
  big.value.limbs[4]=2;
  CHECK(stage2_mA_tBoundedHuge_copy(&big,&big_out)==ASN1_ERR_CONSTRAINT_VIOLATION);
  CHECK(big_out.value.sign==-1 && big_out.value.limbs[4]==1);
  big.value.limbs[4]=0;
  CHECK(stage2_mA_tBoundedHuge_copy(&big,&big_out)==ASN1_ERR_INVALID_ARGUMENT);
  big.value.limbs[4]=1;
  calls=0; fail_at=1;
  CHECK(stage2_mA_tBoundedHuge_copy(&big,&big_out)==ASN1_ERR_OUT_OF_MEMORY);
  fail_at=0;
  stage2_mA_tBoundedHuge_free(&big); stage2_mA_tBoundedHuge_free(&big_out);
  stage2_mA_tBits_init(&bits); stage2_mA_tBits_init(&bits_out);
  bits.value.bits=batch2_malloc(1); CHECK(bits.value.bits);
  bits.value.bits[0]=0xa8; bits.value.bit_length=5; bits.value.cap_bytes=1;
  CHECK(stage2_mA_tBits_copy(&bits,&bits_out)==ASN1_OK && bits_out.value.bits!=bits.value.bits);
  bits.value.bits[0]=0xaa;
  CHECK(stage2_mA_tBits_copy(&bits,&bits_out)==ASN1_ERR_CONSTRAINT_VIOLATION);
  bits.value.bits[0]=0xa8;
  calls=0; fail_at=1;
  CHECK(stage2_mA_tBits_copy(&bits,&bits_out)==ASN1_ERR_OUT_OF_MEMORY); fail_at=0;
  stage2_mA_tBits_free(&bits); stage2_mA_tBits_free(&bits_out);
  stage2_mA_tMode_init(&mode); stage2_mA_tMode_init(&mode_out);
  mode.value=9; CHECK(stage2_mA_tMode_copy(&mode,&mode_out)==ASN1_OK);
  mode.value=3; CHECK(stage2_mA_tMode_copy(&mode,&mode_out)==ASN1_ERR_CONSTRAINT_VIOLATION && mode_out.value==9);
  stage2_mA_tMode_free(&mode); stage2_mA_tMode_free(&mode_out);
  stage2_mA_tIdentifier_init(&oid); stage2_mA_tIdentifier_init(&oid_out);
  oid.data=batch2_malloc(3*sizeof(uint64_t)); CHECK(oid.data); oid.count=oid.capacity=3;
  oid.data[0]=2; oid.data[1]=UINT64_C(4294967296); oid.data[2]=UINT64_MAX;
  CHECK(stage2_mA_tIdentifier_copy(&oid,&oid_out)==ASN1_OK && oid_out.data!=oid.data);
  { stage2_mA_tIdentifier malformed;
    uint64_t only_arc=1;
    stage2_mA_tIdentifier_init(&malformed);
    malformed.data=&only_arc; malformed.count=2; malformed.capacity=1;
    CHECK(stage2_mA_tIdentifier_copy(&malformed,&oid_out)==ASN1_ERR_INVALID_ARGUMENT);
    only_arc=3;
    CHECK(stage2_mA_tIdentifier_copy(&malformed,&oid_out)==ASN1_ERR_INVALID_ARGUMENT);
    CHECK(oid_out.count==3 && oid_out.data[1]==UINT64_C(4294967296));
  }
  oid.data[0]=0; oid.data[1]=40;
  CHECK(stage2_mA_tIdentifier_copy(&oid,&oid_out)==ASN1_ERR_CONSTRAINT_VIOLATION);
  stage2_mA_tIdentifier_free(&oid); stage2_mA_tIdentifier_free(&oid_out);
  stage2_mA_tRelative_init(&rel); stage2_mA_tRelative_init(&rel_out);
  rel.data=batch2_malloc(sizeof(uint64_t)); CHECK(rel.data); rel.count=rel.capacity=1; rel.data[0]=UINT64_MAX;
  CHECK(stage2_mA_tRelative_copy(&rel,&rel_out)==ASN1_OK && rel_out.data[0]==UINT64_MAX);
  stage2_mA_tRelative_free(&rel); stage2_mA_tRelative_free(&rel_out);
  stage2_mA_tReading_init(&real); stage2_mA_tReading_init(&real_out);
  real.value=-0.0; CHECK(stage2_mA_tReading_copy(&real,&real_out)==ASN1_OK);
  CHECK(memcmp(&real.value,&real_out.value,sizeof(double))==0);
  real.value=INFINITY; CHECK(stage2_mA_tReading_copy(&real,&real_out)==ASN1_OK && isinf(real_out.value));
  real.value=NAN; CHECK(stage2_mA_tReading_copy(&real,&real_out)==ASN1_OK && isnan(real_out.value));
  stage2_mA_tPayload_init(&raw); stage2_mA_tPayload_init(&raw_out);
  raw.data=batch2_malloc(1); CHECK(raw.data); raw.count=raw.capacity=1; raw.data[0]=0x80;
  CHECK(stage2_mA_tPayload_copy(&raw,&raw_out)==ASN1_OK && raw.data!=raw_out.data && raw_out.data[0]==0x80);
  stage2_mA_tPayload_free(&raw); stage2_mA_tPayload_free(&raw_out);
  stage2_mA_tFixed_init(&fixed); stage2_mA_tFixed_init(&fixed_out);
  fixed.value.value=1; CHECK(stage2_mA_tFixed_copy(&fixed,&fixed_out)==ASN1_OK && fixed_out.value.value==1);
  stage2_mA_tFixed_free(&fixed); stage2_mA_tFixed_free(&fixed_out);
  CHECK(live==0);
  return 0;
}
static int planning(void) {
  const stage2_field_plan *field;
  uint32_t selected=UINT32_MAX;
  stage2_mA_tPlanning value,copy;
  CHECK(stage2_mA_tPlanning_field_count()==6);
  field=stage2_mA_tPlanning_field(1); CHECK(field);
  CHECK(field->tag_class==1 && field->tag_number==UINT64_C(4294967297) && field->explicit_tag);
  CHECK(field->presence_index==0 && strcmp(field->governor,"id")==0 && field->selector_count==2);
  CHECK(stage2_mA_tPlanning_field(2)->presence_index==1);
  CHECK(stage2_mA_tPlanning_field(3)->extension_group==0 && !stage2_mA_tPlanning_field(3)->nested_sequence);
  CHECK(stage2_mA_tPlanning_field(4)->extension_group==1 && stage2_mA_tPlanning_field(4)->nested_sequence);
  CHECK(stage2_mA_tPlanning_field(5)->presence_index==0);
  CHECK(stage2_mA_tPlanning_field(3)->presence_index==SIZE_MAX);
  CHECK(stage2_mA_tPlanning_field(999)==NULL);
  CHECK(stage2_mA_tPlanning_select(1,"340282366920938463463374607431768211456",&selected)==ASN1_OK);
  CHECK(selected==field->entries[0].type_id);
  CHECK(stage2_mA_tPlanning_select(1,"-7",&selected)==ASN1_OK && selected==field->entries[1].type_id);
  selected=UINT32_MAX;
  CHECK(stage2_mA_tPlanning_select(1,"999",&selected)==ASN1_ERR_CONSTRAINT_VIOLATION && selected==UINT32_MAX);
  CHECK(stage2_mA_tPlanning_select(0,"1",&selected)==ASN1_ERR_UNSUPPORTED && selected==UINT32_MAX);
  stage2_mA_tPlanning_init(&value); stage2_mA_tPlanning_init(&copy);
  value.has_fsingle=1; value.fsingle.value=1;
  CHECK(stage2_mA_tPlanning_copy(&value,&copy)==ASN1_OK);
  value.has_fmaybe=1; value.fmaybe.value=1;
  CHECK(stage2_mA_tPlanning_copy(&value,&copy)==ASN1_ERR_CONSTRAINT_VIOLATION && !copy.has_fmaybe);
  value.has_frequired=1; value.frequired.value=1;
  CHECK(stage2_mA_tPlanning_copy(&value,&copy)==ASN1_OK && copy.frequired.value==1);
  stage2_mA_tPlanning_free(&value); stage2_mA_tPlanning_free(&copy);
  CHECK(live==0);
  return 0;
}
int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1],"--profiles") == 0) return profiles();
  if (argc == 2 && strcmp(argv[1],"--plans") == 0) return planning();
  if (argc == 2 && strcmp(argv[1],"--size") == 0) return size_membership();
  if (argc != 1) return 2;
  stage2_mA_tDefaults defaults, defaults_copy;
  stage2_mA_tNestedDefaults nested;
  stage2_mA_tRecord src, out;
  stage2_mA_tLink link, copy;
  stage2_mA_tDiscrete discrete, discrete_out;
  stage2_mA_tPick bad, old;
  stage2_mA_tRecord_init(&src); stage2_mA_tRecord_init(&out);
  stage2_mA_tDefaults_init(&defaults); stage2_mA_tDefaults_init(&defaults_copy);
  stage2_mA_tNestedDefaults_init(&nested);
  CHECK(defaults.fenabled.value == 1 && defaults.fnumber.value == 7);
  CHECK(defaults.falias.value == 0);
  CHECK(nested.frequired.fenabled.value == 1 && nested.frequired.fnumber.value == 7);
  CHECK(nested.has_foptional == 0 && nested.foptional.fnumber.value == 7);
  defaults.fenabled.value = 0; defaults.fnumber.value = 12;
  CHECK(stage2_mA_tDefaults_copy(&defaults,&defaults_copy) == ASN1_OK);
  CHECK(defaults_copy.fenabled.value == 0 && defaults_copy.fnumber.value == 12);
  stage2_mA_tDefaults_free(&defaults_copy); stage2_mA_tDefaults_free(&defaults_copy);
  CHECK(defaults_copy.fenabled.value == 1 && defaults_copy.fnumber.value == 7);
  stage2_mA_tNestedDefaults_free(&nested);
  CHECK(nested.frequired.fnumber.value == 7 && nested.foptional.fenabled.value == 1);
  CHECK(batch2_second() == 0);
  stage2_mA_tDiscrete_init(&discrete); stage2_mA_tDiscrete_init(&discrete_out);
  discrete.value = 4; discrete_out.value = 7;
  CHECK(stage2_mA_tDiscrete_copy(&discrete,&discrete_out) == ASN1_ERR_CONSTRAINT_VIOLATION);
  CHECK(discrete_out.value == 7);
  discrete.value = 9;
  CHECK(stage2_mA_tDiscrete_copy(&discrete,&discrete_out) == ASN1_OK);
  stage2_mA_tPick_init(&bad); stage2_mA_tPick_init(&old);
  bad.kind = (enum stage2_mA_tPick_kind)999;
  CHECK(stage2_mA_tPick_copy(&bad,&old) == ASN1_ERR_INVALID_ARGUMENT);
  CHECK(old.kind == stage2_mA_tPick_invalid);
  stage2_mA_tPick_free(&bad); stage2_mA_tPick_free(&old);
  CHECK((int)src.fpick.kind == (int)stage2_mA_tPick_invalid);
  src.has_fauto = 1; src.fauto.value = 1;
  src.frestrict.value = 1; src.fa_x2db.value = 1;
  bytes(&src.fdata,11);
  src.fpick.kind = (int)stage2_mA_tPick_arm_ftext;
  bytes(&src.fpick.value.ftext,22);
  src.fbag.data = batch2_calloc(2,sizeof(*src.fbag.data));
  CHECK(src.fbag.data);
  src.fbag.count = src.fbag.capacity = 2;
  bytes(&src.fbag.data[0],33); bytes(&src.fbag.data[1],44);
  bytes(&out.fdata,99);
  {
    const size_t baseline = live;
    unsigned char *old = out.fdata.data;
    size_t failures = 0;
    for (size_t i = 1; i <= 5; ++i) {
      calls = 0; fail_at = i;
      CHECK(stage2_mA_tRecord_copy(&src,&out) == ASN1_ERR_OUT_OF_MEMORY);
      CHECK(live == baseline && out.fdata.data == old && old[0] == 99);
      ++failures;
    }
    CHECK(failures == 5);
  }
  fail_at = 0;
  CHECK(stage2_mA_tRecord_copy(&src,&out) == ASN1_OK);
  CHECK(out.fdata.data != src.fdata.data && out.fdata.data[0] == 11);
  CHECK(out.fpick.value.ftext.data != src.fpick.value.ftext.data);
  CHECK(out.fbag.data != src.fbag.data && out.fbag.data[1].data[0] == 44);
  CHECK(stage2_mA_tRecord_copy(&out,&out) == ASN1_OK);
  stage2_mA_tRecord_free(&out);
  out.fpick.kind = (int)stage2_mA_tPick_arm_fflag; out.fpick.value.fflag.value = 1;
  CHECK(stage2_mA_tRecord_copy(&out,&src) == ASN1_OK);
  CHECK((int)src.fpick.kind == (int)stage2_mA_tPick_arm_fflag);
  stage2_mA_tRecord_free(&src); stage2_mA_tRecord_free(&src);
  stage2_mA_tRecord_free(&out); stage2_mA_tRecord_free(NULL);
  CHECK(live == 0);
  /* Partially built absent members must still be freed. */
  stage2_mA_tLink_init(&link); stage2_mA_tLink_init(&copy);
  link.fnode = batch2_calloc(1,sizeof(*link.fnode)); CHECK(link.fnode);
  link.fnode->has_fnext = 0; link.fnode->fnext = NULL;
  CHECK(stage2_mA_tLink_copy(&link,&copy) == ASN1_OK);
  CHECK(copy.fnode == NULL);
  stage2_mA_tLink_free(&link);
  CHECK(live == 0);
  link.fnode = batch2_calloc(1,sizeof(*link.fnode)); CHECK(link.fnode);
  link.has_fnode = 1; link.fnode->has_fnext = 1;
  link.fnode->fnext = batch2_calloc(1,sizeof(*link.fnode->fnext)); CHECK(link.fnode->fnext);
  stage2_mA_tLink_init(link.fnode->fnext);
  CHECK(stage2_mA_tLink_copy(&link,&copy) == ASN1_OK);
  CHECK(copy.fnode != link.fnode && copy.fnode->fnext != link.fnode->fnext);
  stage2_mA_tLink_free(&link); stage2_mA_tLink_free(&copy);
  CHECK(live == 0);
  /* Depth failure must clean every temporary while retaining the old output. */
  {
    stage2_mA_tLink *tail = &link;
    for (size_t i = 0; i < 260; ++i) {
      tail->fnode = batch2_calloc(1,sizeof(*tail->fnode)); CHECK(tail->fnode);
      tail->has_fnode = 1; tail->fnode->has_fnext = 1;
      tail->fnode->fnext = batch2_calloc(1,sizeof(*tail->fnode->fnext)); CHECK(tail->fnode->fnext);
      stage2_mA_tLink_init(tail->fnode->fnext);
      tail = tail->fnode->fnext;
    }
    const size_t baseline = live;
    CHECK(stage2_mA_tLink_copy(&link,&copy) == ASN1_ERR_RESOURCE_LIMIT);
    CHECK(copy.fnode == NULL && live == baseline);
    stage2_mA_tLink_free(&link);
    CHECK(live == 0);
  }
  return 0;
}
