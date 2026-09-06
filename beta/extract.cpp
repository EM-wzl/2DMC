#include <cstdio>
#include <cstring>
int parse(const char* file, int* vals){
    FILE* fp = fopen(file,"r");
    if(!fp) return -1;
    char buf[16384]; buf[0]=0;
    char line[4096]; bool started=false;
    while(fgets(line,4096,fp)){
        if(!started && strstr(line,"int blocktouch")){ started=true; strcat(buf,line); if(strchr(line,'}')) break; continue; }
        if(started){ strcat(buf,line); if(strchr(line,'}')) break; }
    }
    fclose(fp);
    int n=0;
    char* p = strchr(buf,'{'); if(p) p++;
    int v=0; bool has=false;
    for(; p&&*p; p++){
        if(*p>='0'&&*p<='9'){ v=v*10+(*p-'0'); has=true; }
        else if(*p==','||*p=='}'){ if(has&&n<200) vals[n++]=v; v=0; has=false; if(*p=='}') break; }
    }
    return n;
}
int main(){
    int a[200], b[200];
    int na = parse("beta2.0.cpp", a);   // 当前
    int nb = parse("beta1.8.cpp", b);   // beta1.8 正确值
    printf("n_cur=%d n_18=%d\n", na, nb);
    printf("差异位置: ");
    for(int i=0;i<90;i++){
        int correct = (i<nb)?b[i]:0;
        if(i>=93) continue;
        if(i==93||i==102||i==103) continue;
        if(a[i]!=correct) printf("[%d] cur=%d want=%d  ", i, a[i], correct);
    }
    printf("\n93-103(当前): ");
    for(int i=93;i<=103;i++) printf("%d:%d ", i, a[i]);
    printf("\n");
    return 0;
}
