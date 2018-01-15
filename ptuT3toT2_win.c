/************************************************************************

ptuT3toT2 

Time Gating and Conversion based on below program UPDATED FOR PTU FILES 

updated by Trevor Hull - @trevhull, trevor.d.hull@gmail.com, trevorhull.com

see github.com/trevhull for source

based on:
~~~~~~~~~~

 PicoQuant HydraHarp   -  Conversion Utility  -  M.Wahl, Aug. 2010

  Read a ht3 file and convert it to ht2. Optionally apply a time gate.
~~~~~~~~~~
  UPDATED FOR PTU FILES


************************************************************************/

#include	<stdio.h>
#include	<stddef.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<string.h>
#include	<stdbool.h>
#include	<objbase.h> //tried to only use standard C libraries so it would be windows/linux compatable but had to add this windows one for some GUID writing.

#define MEASMODE_T2  2
#define MEASMODE_T3  3

#define T2WRAPAROUND 33554432 //Only using the v2 T2wraparound, which is probbaly not as good, will hopefully add v1 and someway to detect v1 vs v2 in the future.
#define T3WRAPAROUND 1024

#define T2MODE 66054 //will use this later to tell the program that we got us a T2 not a T3 file. There's an accompanying T3MODE number but we don't need it for this program.

#define TTTRTagTTTRRecType	"TTResultFormat_TTTRRecType"
#define TTTRTagNumRecords	"TTResult_NumberOfRecords"
#define TTTRTagRes		"MeasDesc_Resolution"
#define TTTRTagGlobRes		"MeasDesc_GlobalResolution"
#define FileTagEnd		"Header_End"

// TagTypes  (TTagHead.Typ)
#define tyEmpty8      0xFFFF0008
#define tyBool8       0x00000008
#define tyInt8        0x10000008
#define tyBitSet64    0x11000008
#define tyColor8      0x12000008
#define tyFloat8      0x20000008
#define tyTDateTime   0x21000008
#define tyFloat8Array 0x2001FFFF
#define tyAnsiString  0x4001FFFF
#define tyWideString  0x4002FFFF
#define tyBinaryBlob  0xFFFFFFFF

// TTTR RecordTypes
#define rtPicoHarpT3     0x00010303    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $03 (PicoHarp)
#define rtPicoHarpT2     0x00010203    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $03 (PicoHarp)
#define rtHydraHarpT3    0x00010304    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $04 (HydraHarp)
#define rtHydraHarpT2    0x00010204    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $04 (HydraHarp)
#define rtHydraHarp2T3   0x01010304    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $04 (HydraHarp)
#define rtHydraHarp2T2   0x01010204    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $04 (HydraHarp)
#define rtTimeHarp260NT3 0x00010305    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $05 (TimeHarp260N)
#define rtTimeHarp260NT2 0x00010205    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $05 (TimeHarp260N)
#define rtTimeHarp260PT3 0x00010306    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T3), HW: $06 (TimeHarp260P)
#define rtTimeHarp260PT2 0x00010206    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $06 (TimeHarp260P)

#pragma pack(8) //structure alignment to 8 byte boundaries

// These are substructures used further below 

//These are only for good for hydraharp and timeharp, I guess. Should add picoharp which is slightly different, I think.

typedef union {
				unsigned int  allbits;
				struct	{
					unsigned nsync		:10; 	// numer of sync period
					unsigned dtime		:15;    // delay from last sync in units of chosen resolution 
					unsigned channel	:6;
					unsigned special	:1;
						} bits;					} tT3Rec;


typedef union {
				unsigned int allbits;
				struct	{
					unsigned timetag	:25; 			
					unsigned channel	:6;
					unsigned special	:1; 
						} bits;					} tT2Rec;

//This general taghead is the new ptu style, and the loop used to read/write it was mostly taken from the ptudemo provided by picoquant.
struct TgHd{
	char Ident[32];
	int Idx;
	unsigned int Typ;
	long long TagValue;
	} TagHead;

//This probably didn't need to be a struc, but it was helpfulf or me to understand how structs work for the other tagheads!
struct pream{
	char Magic[8];
	char VVersion[8];
	} PreAmble;

//I don't think this is really used, but it was in the picoquant files and I am too scared to remove it lmao
time_t Result;
const int EpochDiff = 25569;
const int SecsInDay = 86400;

time_t TDateTime_TimeT(double Convertee)
{
  time_t Result = ((long)(((Convertee) - EpochDiff) * SecsInDay));
  return Result;
}




int main(int argc, char* argv[])
{
	int result;
	FILE *fpin,*fpout; 	
	int i, tg_start=-1, tg_end=-1;
	int byteoffs = 0;
	tT3Rec T3Rec;
	tT2Rec T2Rec;
	unsigned int n, truensync=0, oflcorrection = 0, output_oflcorrection = 0, delta, truetime, output_delta, T2delta;
	double NewResolution = 0.0, NewGlobRes = 0.0;
	unsigned long long nht2recs = 0;	
	double GlobRes = 0.0;
	double Resolution = 0;	
	unsigned int syncperiod;
	unsigned long long NumRecords = -1; //equivalent to nht2recs in the ht3tot2 conversion from picoquant
	int NumRecordsIdx = 0;
	long long RecordType = 0;
	char* AnsiBuffer;
	char fileguid;	
	wchar_t* WideBuffer;	
	wchar_t  uffer[40]={0};
	char ffer[40]={0};
	char Buffer[40] = "{";
	time_t CreateTime;
	char* Temp;
	GUID guu = {0};


	
	printf("\nHydraHarp/timeharp PTU T3 to PTU T2 Conversion Tool");
	printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

	
	if((argc<3)||(argc>5)||(argc==4))
	{
usage:
 	 printf("\n");
	 printf("usage: ptuT3toT2 infile oufile [timegate_start timegate_end]\n");
	 printf("infile is a HydraHarp/TimeHarp PTU T3 file (binary)\n"); 
	 printf("outfile is a HydraHarp/TimeHarp PTU T2 file (binary)\n");
	 printf("the optional parameters timegate_start and timegate_end are in picoseconds but it seems like it's not really optional\n");
	 printf("If you just wanna have no time gate put in the time gate as 0 100000000000 or something\n");
	 getchar();
	 exit(-1);
	}
	
	if(argc==5) //time gating
	{
		i = sscanf(argv[3],"%d",&tg_start);
		if((i!=1)||(tg_start<0))
			goto usage;
		i = sscanf(argv[4],"%d",&tg_end);
		if((i!=1)||(tg_end<0))
			goto usage;
		if(tg_start>tg_end)
			goto usage;
	}
		
	if((fpin=fopen(argv[1],"rb"))==NULL)
			{printf("\n ERROR! Input file cannot be opened, aborting.\n"); goto ex;}
		
	if((fpout=fopen(argv[2],"wb"))==NULL)
	 {printf("\n ERROR! Output file cannot be opened, aborting.\n"); goto ex;}
	//This is all just making sure we can read the input file and write to our new output file
	printf("\nLoading data from %s \n", argv[1]);
	printf("\nWriting output to %s \n", argv[2]);

	printf("\n");

	
	
 	result = fread( &PreAmble, 1, sizeof(PreAmble) ,fpin);
	if (result!= sizeof(PreAmble))
	{
	 printf("\n Error reading magic, aborted.");
	   goto close;
	}

	if (strncmp(PreAmble.Magic, "PQTTTR", 6))//The first thing in the binary file is this "Magic" that tells you its a TTTR file. 
	{
	 printf("\n Wrong magic, not a ptu file.");
	   goto close;
	}
	result = fwrite( &PreAmble,1, sizeof(PreAmble), fpout);
		if (result!= sizeof(PreAmble))
		{
		 printf("\nerror writing preamble");
		 goto close;
		}


//	printf("\n Magic: %s\n", PreAmble.Magic);
//	printf("\n Tag Version: %s\n", PreAmble.VVersion);
//You don't really need to print these but it was useful for debugging



/*
Alright so here's the TagHead part, basically we're just gonna read the tag from the picoquant file, and write it to our new binary file.
There are only a few thing we have to change so that it will recognize it as a T2 file and read it correctly, whenever possible we're just not gonna touch the info

*/
	do{
		
	
	result = fread( &TagHead, 1, sizeof(TagHead), fpin);
	if (result!= sizeof(TagHead))
	{
		printf("\nIncomplete File.");
			goto close;
	}
	
	

//this next part was in the picoquant file, I don't know why though, it seems to work without it but maybe something is wrong so I'm leaving it commented.	
/*	strcpy(Buffer, TagHead.Ident);
	if (TagHead.Idx > -1)
	{
	 sprintf(Buffer, "%s(%d)", TagHead.Ident,TagHead.Idx);
	}
	printf("\n%-40s", Buffer);
*/
		
//All these types are described by some picoquant documentation in the ptudemo files, it's kind of useful but also, I found, pretty confusing. but you should check there if you have questions.
	switch (TagHead.Typ)
	    {
		case tyEmpty8:
		
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing tyEmpty8 Tag");
		 goto close;
		}
		break;
	      case tyBool8:
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing bool Tag");
		 goto close;
		}

		break;
	      case tyInt8:
		// get some Values we need to analyse records
		if (strcmp(TagHead.Ident, TTTRTagNumRecords)==0) // Number of records
			  {  NumRecords = TagHead.TagValue;
				NumRecordsIdx = TagHead.Idx;
				byteoffs = ftell(fpout); //keep for update of Header after we perform the time gate and T2 conversion the number of records might be different. e.g. gated photons and hte # of overflows is def different.
			}

		if (strcmp(TagHead.Ident, TTTRTagTTTRRecType)==0) // TTTR RecordType
			    TagHead.TagValue = rtTimeHarp260PT2; // This should be like an if loop to catch all the different formats, e.g. HydraHarp, etc. but mine is a TimeHarp so I should generalize this.
		
		if (strcmp(TagHead.Ident,"Measurement_Mode" )==0) // TTTR RecordType
			{
				if(TagHead.TagValue != MEASMODE_T3)	
			 	{
				printf("\n input must be T3");
				goto close;
				}
			   TagHead.TagValue = MEASMODE_T2; //We gotta tell the software that the new file is T2 not T3, easy peasy.
			}
//		if (strcmp(TagHead.Ident, "TTResult_SyncRate")==0)			
//			TagHead.TagValue = 0;
// So there shouldn't really be a sync rate for T2 measurements, although it's possible. It doesn't really matter, though. I'm leaving this commented because we don't use sync rate anywhere as far as I can tell so, no harm no foul.
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing int8 Tag");
		 goto close;
		}
		break;
	      case tyBitSet64:
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing tyBitSet64  Tag");
		 goto close;
		}
		break;
	      case tyColor8:
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing tyColor64  Tag");
		 goto close;
		}
		break;

//Alright this stuff is pretty important
	      case tyFloat8:
		
		if (strcmp(TagHead.Ident, TTTRTagRes)==0) // Resolution for TCSPC-Decay
		       {   Resolution = *(double*)&TagHead.TagValue; // I have no idea why it has to have all those * asterisks, something to do with pointers, if you understand C better than me please let me know!
			}// Anyway we gotta get this Resolution so we can convert the Dtime from T3 into seconds. the Resolution can change based on the measurements, but it's recorded in the tagheader and we're gonna grab it right here.
		
		if (strcmp(TagHead.Ident, "HW_BaseResolution")==0)
			NewGlobRes = *(double*)&TagHead.TagValue;// apparently the T2 resolution is hardware based and is always the same for diff hardware. The global resolution can change for T3, but not T2

/* From picoquant support:
	 "MeasDesc_Resolution, and MeasDesc_BinningFactor have meaning only in T3 mode. LEave them as they saved in the original T3 mode data file. MeasDesc GlobalREsolution is 1ps in HydraHarp400. More precisely, stored as 1.000000e-012 or a number very close to this.
	In general, time tagging in T2 mode is always done iwthe the best time resolution the device can provide and that value is stored as MeasDesc_GlobalResolution. 1ps for HH400, 4ps for PH300, 25ps for TH260Pico, 250ps or 1ns for TH260Nano."
*/

		if (strcmp(TagHead.Ident, TTTRTagGlobRes)==0) // Global resolution for timetag
		        {    GlobRes = *(double*)&TagHead.TagValue; // We need the T3 resolution as written from picoquant software to interpret T3 records, but then below we'll change the resolution so the new T2 file can be read.
				TagHead.TagValue =( *(long long*)&NewGlobRes);	//Here we're making a new glob res (MeasDesc_GlobalResolution) for our T2 global resolution. so we take the hardware resolution we read above and  write it as the new T2 Global Resolution 
			}
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing tyFloat8  Tag");
		 goto close;
		}	



		break;
	      case tyFloat8Array:
	//	printf("<Float Array with %d Entries>", TagHead.TagValue / sizeof(double));
		// only seek the Data, if one needs the data, it can be loaded here 
		// I don't really know what's going on here so we're just gonna take it and write it to the new file and hope for the best
		fseek(fpin, (long)TagHead.TagValue, SEEK_CUR);
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing tyFloat8Array  Tag");
		 goto close;
		}
		break;
	      case tyTDateTime:
		//time_t CreateTime;
//		CreateTime = TDateTime_TimeT(*((double*)&(TagHead.TagValue)));
//		printf("\ntime is %s", asctime(CreateTime)); this segfaults :(
//		printf("\ntimeis %ld", CreateTime);
//		printf("\n TDateTime");
		
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing time Tag");
		 goto close;
		}

		break;
	      case tyAnsiString:
		AnsiBuffer = (char*)calloc((size_t)TagHead.TagValue,1);				//This took me a while to figure out so I'm gonna describe it here. In some cases the TagHead.TagValue is the actual value you're looking for, but for the ASCII or ANSI strings it's not. 
		        result = fread(AnsiBuffer, 1, (size_t)TagHead.TagValue, fpin);		//The actual Ansi string comes right after the Tag, and the TagValue tells you how many bytes the string is, so you create this new AnsiBuffer that is the size of the string (or the number the tagValue gives you
		      if (result!= TagHead.TagValue)						//Then you read that number of bytes into the Ansi string, which reads you the string! then right after that string is done (or the number of bytes from the TagValue) the next Tag starts.
		{
		  printf("\n broken ansi string");
		 	free(AnsiBuffer);
		      	goto close;
		}
		if( strncmp(TagHead.Ident, "CreatorSW_Name", 14)==0 )
			strcpy(AnsiBuffer, "ptuT3toT2");		//So we gotta tell the instrument and any people that the new file was written by this program, not by Symphotime. So here's that. If someobyd says "This data is strange" They'll see it was made by this program.
									//right so we strncmp to find the Creator name, and then we give the Buffer the new name of ptuT3toT2
		if( strncmp(TagHead.Ident, "File_GUID", 9)==0)
			{						//Symphotime uses the File_GUID to refer to each file, every file has to have a unique GUID, so if you use the same GUID as the T3 file you can't load both files. for example you also can't load two files with different time gating
			CoCreateGuid(&guu);				//To get around this we'll generate a new GUID for each file, it doesn't matter as long as it's unique so you can load multiple files at a time. This bit of code generate the code
			result = StringFromGUID2(&guu,uffer,40);
			if (result==0)					//This is the only part of the code that is not platform agnostic, the only difference between ptuT3toT2_linux and ptuT3toT2_win :/ I'm not happy about it!
				{
				printf("\nStringFromGUID2 failed");
				goto close;
				}
			wcstombs(ffer,uffer,sizeof(uffer));		//which speaking of curly brace its windows default and, like, impossible to make on linux :( or not impssible but I had to manually add it >:/
			strcpy(AnsiBuffer,ffer);
			}
				
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
 			if (result!= sizeof(TagHead))				//Here we're writing the tag, and we're not really changing anythying, probably the TagValue should be changed to the new size of the ANSI string, but it was easier to just leave the string the same size. probably some empty characters
			{
			 printf("\nerror writing NewTag.");
			goto close;
			}
		result = fwrite(AnsiBuffer, 1, (size_t)TagHead.TagValue,fpout);  //right after you write the Tag, write the actual ANSI string. and make sure the size of the string is the same szie as the TagVAlue
			if (result!=(size_t)TagHead.TagValue)
			{
			 printf("\nerror writing enhancement");
			goto close;
			}
		
		
		free(AnsiBuffer); //lets clear that buffer memory otherwise computer gets mad at you
		break;
	
		 case tyWideString:	//same deal as above with the ANSI stringh but this Widestring needs different type definitions and stuff. Also we don't change anything here so if you see a Widestring, just write the tag and then write the string.
		WideBuffer = (wchar_t*)calloc((size_t)TagHead.TagValue,1);
		        result = fread(WideBuffer, 1, (size_t)TagHead.TagValue, fpin);
		      if (result!= TagHead.TagValue)
		{
		  printf("\nerror reading WideString.");
		  free(WideBuffer);
		          goto close;
		}
	
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
 			if (result!= sizeof(TagHead))
			{
			 printf("\nerror writing NewTag.");
			goto close;
			}
		result = fwrite(WideBuffer, 1, (size_t)TagHead.TagValue,fpout);
			if (result!=(size_t)TagHead.TagValue)
			{
			 printf("\nerror writing enhancement");
			goto close;
			}


		free(WideBuffer);
		break;
		    case tyBinaryBlob:
		// only seek the Data, if one needs the data, it can be loaded here
		result = fwrite( &TagHead, 1, sizeof(TagHead), fpout);
		if (result!= sizeof(TagHead))
		{
		 printf("\nerror writing binaryblob Tag");
		 goto close;
		}
//		fseek(fpin, (long)TagHead.TagValue, SEEK_CUR);
		break;
	      default:
		printf("\nIllegal Type identifier found! Broken file?");
		goto close;
	    }

	
	}
	while((strncmp(TagHead.Ident, FileTagEnd, sizeof(FileTagEnd))));	//do the whole tag loop until you read the tag that says "TagEnd" or whatever. Now we'll go onto the photon events. We did it! We read the TagHeaders!









//uh not dealing with this rn
//	if(  strncmp(TxtHdr.Version,"1.0",3)  )
//	{
//	   printf("\nError: File format version is %s. This program is for v. 1.0 only.", TxtHdr.Version);
//	   goto ex;
//	}


	if((tg_start>0) && (tg_end>0))
		printf("\n applying time gate from %d to %d ps\n",tg_start, tg_end);
	
	//now read and interpret the event records

	for(n=0; n<NumRecords; n++)
	{
		i=0;  
		result = fread(&T3Rec.allbits,sizeof(T3Rec.allbits),1,fpin); 
		if(result!=1)
		{
			if(feof(fpin)==0)
			{
				printf("\nerror in input file! \n");
				goto ex;
			}
		}

		if(T3Rec.bits.special==1) 
		{
			if(T3Rec.bits.channel==0x3F) //overflow
			{
				oflcorrection+=(T3WRAPAROUND*T3Rec.bits.nsync);	//So the new overflow compression thing writes hte # of overflows into the nsync, so if you have 4 overflows between photon syou get a special overflow with nsync = 4 (special = 1, channel = 0x3F or 63)
			}
		
			if((T3Rec.bits.channel>=1)&&(T3Rec.bits.channel<=15)) //markers
			{
				truensync = oflcorrection + T3Rec.bits.nsync; 
				printf("marker");
				//the time unit depends on sync period which can be obtained from the file header
				//I don't have markers so I didn't really look at this too much....
			}

		}
		else //regular input channel
		 {
			 truensync = oflcorrection + T3Rec.bits.nsync;
			// alright this stuff gets a little dicey, I'll try to explain it well
			// so the actual sync number needs to be corrected for the overflow.
			// The real data only says what the syn csince the last overflow is, but we want to convert to ACTUAL time and the convert back to T2 OVerflow stuff, so we need to find the actual nsync number so we can convert to ACTUAL time below 

			 //in HALF picoseconds because T2 data is in half picoseconds as of file format version 1.0 (THis was in the ht3toht2 file from picoquant. I think how I set it up is correct, ignoring the half picoseconds thing I'm just gonna give it the real resolution number
			 truetime = (((truensync*GlobRes) + (Resolution*T3Rec.bits.dtime))/NewGlobRes);
			 //so the ACTUAL time of each photon arrival is the actual nsync (i.e. corrected for overflows)*MeasDesc_GlobalResolution. This changes in T3 mode but is written in the TagHead. Then we add the DTime (Dtime is the time in picoseconds the photon arrives AFTER the nsync)
			 //for T2 there is no dtime its just the absolute arrival time so we add those two together. But then we divide by our NEW Globla resolution (which we wrote to MeasDesc_GlobalResolution) so that the software will get the right number when it multiplies by the GlobalResolution
			// I'm like 90% sure this works lmao






			 if( (T3Rec.bits.dtime*Resolution*1e12 >= tg_start) && (T3Rec.bits.dtime*Resolution*1e12 <= tg_end) ) //apply time gate. timegate is in ps so multiply dtime by MeasDesc_Resolution(usually like 50 ps or 5x10^-11 or so) to get to sec then multiply by 1e12 to get to ps.
			 {
				 //check for HT2 oferflow
			//So we get the actual arrival time to convert to T2 but we gotta back out of actual time and use T2 oveflows for the Symphotime or whatever to read it correctly. So delta is the actual arrival time corrected WITH overflows.		
				 delta = truetime - output_oflcorrection;
				output_delta = 0;
			// This is supposed to add the overflow compression (i.e. can write 2 to the timetag if there are two overflows between photons) but it doesn't work. As far as I can tell overflow compression is less important in T2 mode since there are fewer overflows, but a better while or if loop should
			// be made to make this more general. Right now it only writes 1 since it mostly works fine.
				 if (delta>=T2WRAPAROUND) //must insert an overflow record
				 {
					do
					{
					output_delta++;	//output delta would be what you write as the timetag for overflow compression but I can't get the loop to actually loop without writing, will hopefully fix this.
					output_oflcorrection +=T2WRAPAROUND;
					delta = truetime - output_oflcorrection;
					}while(delta>=T2WRAPAROUND);
					T2Rec.bits.special = 1; 
					T2Rec.bits.channel = 0x3F; 
					T2Rec.bits.timetag = output_delta;
					output_delta=0;
					result = fwrite(&T2Rec.allbits,sizeof(T2Rec.allbits),1,fpout); 
					if(result!=1)
					{
						printf("\nerror writing to output file! \n");
						goto ex;
					}
					nht2recs++;

//					output_oflcorrection += T2WRAPAROUND;

//					delta = truetime - output_oflcorrection;
				 }

				 //populate and store the PTU T2 record
				T2Rec.bits.special = 0; 
				T2Rec.bits.channel = T3Rec.bits.channel;
				T2Rec.bits.timetag =(unsigned)delta;

				result = fwrite(&T2Rec.allbits,sizeof(T2Rec.allbits),1,fpout); 
				if(result!=1)
				{
					printf("\nerror writing to output file! \n");
					goto ex;
				}

				nht2recs++;
			 }

		 }
	}


	//finally we must insert the new number of records in the file header


	fseek(fpout,byteoffs,SEEK_SET);
	strcpy(TagHead.Ident,TTTRTagNumRecords);
	TagHead.Idx = NumRecordsIdx;
	TagHead.Typ = tyInt8;
	TagHead.TagValue = nht2recs;
//	printf("\n%s",TagHead.Ident);
//	printf("\n%lld",TagHead.TagValue);	
//	printf("\n%lld",nht2recs);	
	//Those prints above are just to make sure we counted the records correctly.
	result = fwrite( &TagHead, 1, sizeof(TagHead) ,fpout);
	if (result!= sizeof(TagHead))
	{
	  printf("\nerror updating TTTRHdr, aborted.");
	  goto close;
	}
	  goto ex;


close: 
	fclose(fpin);
	fclose(fpout); 

ex:
	printf("\n press enter to exit");
	getchar();
	exit(0);
	return(0);		
}
