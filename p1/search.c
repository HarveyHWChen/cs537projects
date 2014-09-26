/* Created by Hanwen Chen 1-28-2014 */
/* 9069978907 */
/* University of Wisconsin-Madison */
/* CS537 Project 1 */


/*search <input File Number> <key word> <input file 1> <input file 2> ... <output file>*/
/*argv[1]: input File Number*/
/*argv[2]: key word*/
/*argv[3] to argv[3+inputFileNum]: input file names*/
/*argv[argc-1]: output file name*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

int main(int argc, const char * argv[])
{
  int k = 0;
  int outEN = 0;
  /* Check the number of params*/
  if(argc<4 || (argc-4!=atoi(argv[1]) && argc-3!=atoi(argv[1])) || atoi(argv[1])==0)
    {
      fprintf(stderr,"Usage: search <input-number> <key-word> <input-list> <output>\n");
      exit(1);
    } 
  /* Get number of input files*/
  int inFileNum = atoi(argv[1]);
  /* Get key word*/
  char* keyword = (char*) malloc(sizeof(char)*strlen(argv[2]));
  strcpy(keyword,argv[2]);
  int keywordLen = strlen(keyword);
  /* Get input file names*/
  char** inFNames = (char**) malloc(inFileNum*sizeof(char*));
  for(k = 0; k<inFileNum; k++)
    {
      inFNames[k] = (char*) malloc(sizeof(char)*NAME_MAX);
      strcpy(inFNames[k],argv[k+3]);
    }
  /* Get output file name(if applicable)*/
  char *outFName = (char*)malloc(sizeof(char)*NAME_MAX);
  if(argc-inFileNum==4)
    {
      outEN = 1;
      strcpy(outFName,argv[argc-1]);
      /* Check if input file is same as output*/
      for(k=0; k<inFileNum; k++)
	{
	  if(strcmp(argv[k+3],outFName)==0)
	    {
	      fprintf(stderr,"Input and output file must differ\n");
	      exit(1);
	    }
	}
      char realInName[PATH_MAX];
      char realOutName[PATH_MAX];
      realpath(outFName,realOutName);
      /* Check if input and output's realpath are same */
      for (k=0; k<inFileNum; k++)
	{
	  realpath(inFNames[k],realInName);
	  if(strcmp(realOutName,realInName)==0)
	    {
	      fprintf(stderr,"Input and output file must differ\n");
	      exit(1);
	    }
	}
    }
  int *Occur = (int*) malloc(sizeof(int)*inFileNum);
  for(k=0;k<inFileNum;k++)
    {
      /* Open input file */
      FILE *pFile;
      Occur[k] = 0;
      pFile = fopen(inFNames[k],"r");
      if(pFile==NULL)
	{
	  fprintf(stderr,"Error: Cannot open file '%s'\n",inFNames[k]);
	  exit(1);
	}
      /* Get file length */
      int startPos = (int)ftell(pFile);
      fseek(pFile,0,SEEK_END);
      int endPos = (int)ftell(pFile);
      int fileLength = endPos - startPos;
      fseek(pFile,0,SEEK_SET);
      /* Read in the whole file*/
      char *pFileContent = (char*) malloc(sizeof(int)*fileLength);
      int readInNum = fread(pFileContent,sizeof(char),fileLength,pFile);
      if(readInNum!=fileLength)
	{
	  fprintf(stderr,"File read Error!\n");
	  exit(1);
	}
      /* Find matches */
      char *pMatch = pFileContent;
      while((pMatch=strstr(pMatch,keyword)) != NULL)
	{
	  pMatch = pMatch+keywordLen*sizeof(char);
	  Occur[k]++;
	}
      fclose(pFile);
    }
  /* Sort results */
  /* Use insertion sorts */
  int i;
  int j;
  int occurTemp;
  char* nameTemp = (char*)malloc(NAME_MAX*sizeof(char));
  for(i=1;i<=inFileNum-1;i++)
    {
      strcpy(nameTemp,inFNames[i]);
      occurTemp = Occur[i];
      j = i-1;
      while((j>=0)&&(Occur[j]<occurTemp))
	{
	  Occur[j+1] = Occur[j];
	  strcpy(inFNames[j+1],inFNames[j]);
	  j--;
	}
      Occur[j+1] = occurTemp;
      strcpy(inFNames[j+1],nameTemp);
    }
  /* Print the results */   
  if(outEN == 0)
    {
      for(k=0;k<inFileNum;k++)
	{
	  printf("%d %s\n",Occur[k],inFNames[k]);
	}
    }
  /* Save the results to file */
  if(outEN == 1)
    {
      /* Open or create output file */
      FILE *outFile = fopen(outFName,"w");
      if(outFile==NULL)
	{
	  fprintf(stderr,"Error: Cannot open file '%s'\n",outFName);
	  exit(1);
	}
      for(k=0;k<inFileNum;k++)
	{
	  fprintf(outFile,"%d %s\n",Occur[k],inFNames[k]);
	}
      fclose(outFile);
    }
  /* Free pointers created by malloc() */
  free(inFNames);
  free(Occur);
  free(nameTemp);
  free(outFName);
  return 0;
}

