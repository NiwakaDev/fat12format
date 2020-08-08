//fat12フォーマットソフト
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <stdio.h>
#include <sys/stat.h>
#pragma pack(1)
typedef struct _BPB{
    unsigned char BS_JmpBoot[3];
    unsigned char BS_OEMName[8];
    unsigned short BPB_BytePerSec;
    unsigned char BPB_SecPerClus;
    unsigned short BPB_RsvdSecCnt;
    unsigned char BPB_NumFATs;
    unsigned short BPB_RootEntCnt;
    unsigned short BPB_TotSec16;
    unsigned char BPB_Media;
    unsigned short BPB_FATSz16;
    unsigned short BPB_SecPerTrk;
    unsigned short BPB_NumHeads;
    unsigned int BPB_HiddSec;
    unsigned int BPB_TotSec32;
}BPB;
//ディレクトリ構造体
typedef struct _DirEntry{
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char DIR_NTRes;
    unsigned char DIR_CrtTimeTenth;
    unsigned short DIR_CrtTime;
    unsigned short DIR_CrtDate;
    unsigned short DIR_LstAccDate;
    unsigned short DIR_FstClusHI;
    unsigned short DIR_WrtTime;
    unsigned short DIR_WrtDate;
    unsigned short DIR_FstClusLO;
    unsigned int DIR_FileSize;
}DirEntry;
#define FILE_SIZE 1474560
#define START_FILE_REGION 16896 //ファイル領域の開始番地(オフセット)
#define START_DIR_REGION  9728  //ディレクトリ領域の開始番地(オフセット)
#define START_FAT_REGION  5120  //FAT領域の開始番地(オフセット)

//バッファの中身を0にする。
void clear_buff(char *buff){
    int i;
    for(i=0; i<FILE_SIZE; i++){
        *(buff+i) = 0;
    }
}

//BPBをセットする
void SetBPB(char *buff, char *filename){
    BPB bpb;
    FILE *fp;
    char *bpb_buff=malloc(512);
    int i;

    fp = fopen(filename, "rb");
    fread(bpb_buff, 1, 512, fp);
    for(i=0; i<512; i++){
        *(buff+i) = *(bpb_buff+i);
    }
    return;
}

long GetFileSize(char *file)
{
    struct stat statBuf;

    if (stat(file, &statBuf) == 0)
        return statBuf.st_size;

    return -1;
}

//クラスタ数を求める。
unsigned int CalClusterNumber(int file_size){
    unsigned int quotient;//商
    unsigned int remainder;//余り

    quotient  = file_size/512;
    remainder = file_size%512;
    if(remainder>=1){
        quotient += 1;
    }

    return quotient;
}

//ディレクトリ領域をセットする
void SetDirEntry(char *buff, char *filename){
    char* dir_entry = malloc(sizeof(DirEntry));
    DirEntry *dir_entry_pointer = (DirEntry*)dir_entry;

    strcpy(dir_entry, filename);
    dir_entry_pointer->DIR_FstClusLO = 0x0002;//開始クラスタ番号
    printf("ファイル名は%s\n", dir_entry);
    int i;

    for(i=0; i<sizeof(DirEntry); i++){
        *(buff+9728+i) = *(dir_entry+i);
    }
    return;
}

//FAT領域をセットする
void SetFat(char *buff, char *filename){
    long file_size = GetFileSize(filename);
    unsigned int number_of_cluster = CalClusterNumber(file_size);
    unsigned short fat12_region[3072];
    unsigned short cluster_number;
    int i, j;
    FILE *fp;
    char *os_buff=malloc(file_size);
    for(i=0; i<file_size; i++){
        *(os_buff+i) = 0;
    }
    fp = fopen(filename, "rb");
    if(fp==NULL){
        printf("%sは存在しません。\n", filename);
        exit(0);
    }
    fread(os_buff, 1, file_size, fp);
    for(i=0; i<3072; i++){
        fat12_region[i] = 0;
    }
    fat12_region[0] = 0xff0;//fat12仕様
    fat12_region[1] = 0xfff;//fat12仕様

    printf("必要なクラスタ数は、%d\n", number_of_cluster);
    for(i=0; i<number_of_cluster-1; i++){
        fat12_region[2+i] = 0x0002+(i+1);
    }
    fat12_region[2+number_of_cluster-1] = 0x0fff;//ファイルの終わり
    short a1;
    short a2;

    char low;
    char middle;
    char high;

    j=0;
    //FAT用にfat配列を圧縮する。
    for(i=0; i < 3071; i+=2){
        a1 = fat12_region[i];
        a2 = fat12_region[i+1];

        low    = (char)((a1) & 0x00FF);
        middle = (char)((((a1>>8)&0x000F) | ((a2<<4)&0x00F0)) & 0x00FF);
        high   = (char)((a2>>4) & 0x00FF);
        *(buff+START_FAT_REGION+j+0) = low;
        *(buff+START_FAT_REGION+j+1) = middle;
        *(buff+START_FAT_REGION+j+2) = high;
        j += 3;
    }
    cluster_number = 0x002;
    //ディスクイメージにファイルを書き込む
    for(i=0; i<number_of_cluster; i++){
        for(j=0; j<512; j++){
            *(buff+START_FILE_REGION+512*i+j) = *(os_buff+512*i+j);
        }
    }
}

int main(int argc, char **arg){
    char *filename = arg[1];
    FILE *fp;
    char *buff;
    int i;

    printf("BPB構造体のサイズは%lu\n", sizeof(BPB));
    printf("DirEntry構造体のサイズは%lu\n", sizeof(DirEntry));

    if(argc < 2){
        printf("実行時に、ファイル名を入力してください。\n");
        exit(0);
    }
    if(strlen(filename)>8){
        printf("ファイル名は拡張子含めて8文字までにしてください。\n");
        exit(0);
    }
    //FILE_SIZEbyte分の容量を用意する。
    buff = malloc(FILE_SIZE);
    clear_buff(buff);

    fp = fopen(filename, "wb+");

    SetBPB(buff, "ipl.bin");
    SetDirEntry(buff, "nt-os.bin");
    SetFat(buff, "nt-os.bin");
    fwrite(buff, 1, FILE_SIZE, fp);

    fclose(fp);
    printf("フォーマットしました。\n");
    printf("%sのファイルサイズは、%ldです。\n", "nt-os.bin",GetFileSize("nt-os.bin"));

    return 0;
}