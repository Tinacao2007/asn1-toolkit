#include <asn1/c_runtime/value_content.h>
#include <string.h>
static int leap(int year){return year%4==0&&(year%100!=0||year%400==0);}
static int days(int year,int month){static const int table[]={31,28,31,30,31,30,31,31,30,31,30,31};return table[month-1]+(month==2&&leap(year));}
typedef struct {int year,month,day,hour,minute,second,offset;} instant_t;
static int decimal(const uint8_t* p){return (int)(p[0]-'0')*10+(int)(p[1]-'0');}
static asn1_status_t parse(asn1_byte_view_t text,instant_t* t){
  if(!text.data)return ASN1_ERR_INVALID_ARGUMENT;
  const size_t n=text.size;
  if(n!=11&&n!=13&&n!=15&&n!=17)return ASN1_ERR_CONSTRAINT_VIOLATION;
  const int z=text.data[n-1]=='Z';size_t count=z?n-1:n-5;
  if(count!=10&&count!=12)return ASN1_ERR_CONSTRAINT_VIOLATION;
  for(size_t i=0;i<count;++i)if(text.data[i]<'0'||text.data[i]>'9')return ASN1_ERR_CONSTRAINT_VIOLATION;
  const int yy=decimal(text.data);t->year=yy>=50?1900+yy:2000+yy;
  t->month=decimal(text.data+2);t->day=decimal(text.data+4);t->hour=decimal(text.data+6);t->minute=decimal(text.data+8);t->second=count==12?decimal(text.data+10):0;t->offset=0;
  if(t->month<1||t->month>12||t->day<1||t->day>days(t->year,t->month)||t->hour>23||t->minute>59||t->second>59)return ASN1_ERR_CONSTRAINT_VIOLATION;
  if(!z){if(text.data[count]!='+'&&text.data[count]!='-')return ASN1_ERR_CONSTRAINT_VIOLATION;
    for(size_t i=count+1;i<n;++i)if(text.data[i]<'0'||text.data[i]>'9')return ASN1_ERR_CONSTRAINT_VIOLATION;
    const int hours=decimal(text.data+count+1),minutes=decimal(text.data+count+3);if(hours>23||minutes>59)return ASN1_ERR_CONSTRAINT_VIOLATION;
    t->offset=(hours*60+minutes)*(text.data[count]=='-'?-1:1);
  }return ASN1_OK;
}
asn1_status_t asn1_utctime_validate(asn1_byte_view_t text){instant_t value;return parse(text,&value);}
asn1_status_t asn1_utctime_normalize(asn1_byte_view_t text,uint8_t out[13]){
  if(!out)return ASN1_ERR_INVALID_ARGUMENT;instant_t t;asn1_status_t s=parse(text,&t);if(s!=ASN1_OK)return s;
  int minutes=t.hour*60+t.minute-t.offset;
  if(minutes<0){minutes+=1440;if(--t.day==0){if(--t.month==0){t.month=12;--t.year;}t.day=days(t.year,t.month);}}
  else if(minutes>=1440){minutes-=1440;if(++t.day>days(t.year,t.month)){t.day=1;if(++t.month==13){t.month=1;++t.year;}}}
  if(t.year<1950||t.year>2049)return ASN1_ERR_CONSTRAINT_VIOLATION;
  const int parts[]={t.year%100,t.month,t.day,minutes/60,minutes%60,t.second};uint8_t tmp[13];
  for(size_t i=0;i<6;++i){tmp[2*i]=(uint8_t)('0'+parts[i]/10);tmp[2*i+1]=(uint8_t)('0'+parts[i]%10);}tmp[12]='Z';memcpy(out,tmp,13);return ASN1_OK;
}
