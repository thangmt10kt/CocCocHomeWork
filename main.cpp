#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <algorithm>   
#include <climits>
#include <iostream>

using namespace std;

void createChunkFile(int chunkIdx, vector<string> &strs)
{
	char buffNum[33];
	itoa(chunkIdx, buffNum, 10);
	fstream chunkFile;
	
	chunkFile.open(buffNum, ios::out);
	sort(strs.begin(),strs.end());
	/* Write string to chunk file */
	for(string str:strs)
	{
		chunkFile << str << endl;
	}
	chunkFile.close();
}

/* Refill merge buffer */
inline void fileMergeBuff(fstream &file, vector<string> &vecStrs, string &pend, long long int memLimit)
{
	string lineData;
	long long int usedMem = 0;
	char stopFlag = 0;
	
	vecStrs.clear();
	
	/* In case a line is not put to merge buffer */
	if(pend.size() != 0)
	{
		vecStrs.push_back(pend);
		usedMem = pend.size();
	}

	pend.clear();

	/* Update the merge buffer */
	while(getline(file, lineData))
	{
		if((usedMem + lineData.size()) <= memLimit)
		{
			try
			{
				vecStrs.push_back(lineData);
				usedMem = usedMem + lineData.size() + sizeof(string);
			}
			catch (std::bad_alloc& ba)
			{
				pend = lineData;
				stopFlag = 1;
			}
		}
		else
		{
			pend = lineData;
			stopFlag = 1;
		}
		
		if(stopFlag == 1)
		{
			break;
		}
	}
}


int main(int argc, char **argv)
{
	long long int availMemSize = 0;
	char *inputFileName = argv[1];
	char *outputFileName = argv[2];
	fstream ipFile;
	fstream opFile;
	long long int usedSize = 0;
	int cntChunk = 0;
	char buffNum[33];

	/* get the memory size */
	sscanf(argv[3], "%lld", &availMemSize);

	/* Only use 3/2 available memory to ensure the health of software */
	availMemSize = availMemSize*3/4;

	/* Distribute input file to chunk files */
	{
		vector<std::string> lineStr;
		std::string tp;
		
		ipFile.open(inputFileName, ios::in);

		while(getline(ipFile, tp))
		{
			/* Get string from input file then sort them*/
			if((usedSize + tp.size()) <= availMemSize)
			{
				try{
					lineStr.push_back(tp);
					usedSize += tp.size() + sizeof(string);
				}
				catch(std::bad_alloc& ba)
				{
					/* Create new chunk file in case the allocation is fail */
					cntChunk++;
					createChunkFile(cntChunk, lineStr);
					lineStr.clear();
					lineStr.push_back(tp);
					usedSize = tp.size() + sizeof(string);
				}
			}
			else{
					cntChunk++;
					createChunkFile(cntChunk, lineStr);
					lineStr.clear();
					lineStr.push_back(tp);
					usedSize = tp.size() + sizeof(string);
			}
		}
		ipFile.close();

		/* Write remaining string to chunk file */
		if(lineStr.size() != 0)
		{
			cntChunk++;
			createChunkFile(cntChunk, lineStr);
			lineStr.clear();
			usedSize = 0;
		}
	}

	/* Define variable to manage merge buffers */
	vector<fstream> chunkFiles(cntChunk);
	vector<vector<string>> multiMergWay(cntChunk);
	vector<int> checkingPoint(cntChunk, 0);
	vector<string> pendingStr(cntChunk);
	vector<string> sortBuf;
	char firstLineFlag = 0;
	long long int sortBufSize = 0;
	long long int chunkSize = availMemSize / (cntChunk + 1);

	/* Clear old content */
	opFile.open(outputFileName, ios::out|ios::trunc);
	opFile.close();

	opFile.open(outputFileName, ios::app);

	/* Open all chunk file */
	for(int idx = 0; idx < cntChunk; ++idx)
	{
		itoa(idx + 1, buffNum, 10);
		chunkFiles[idx].open(buffNum, ios::in);
	}

	/* Load data from chunk files to merge buffers */ 
	for(int idx = 0; idx < cntChunk; ++idx)
	{
		fileMergeBuff(chunkFiles[idx],multiMergWay[idx], pendingStr[idx], chunkSize);
	}
	
	/* Merge all buffer */
	while(chunkFiles.size() > 0)
	{
		string smallestStr = multiMergWay[0][checkingPoint[0]];
		int smlIdx = 0;
		/* Find the smallest string in merge buffers */
		for(int idx = 1; idx < chunkFiles.size(); ++idx)
		{
			if(smallestStr.compare(multiMergWay[idx][checkingPoint[idx]]) > 0)
			{
				smlIdx = idx;
				smallestStr = multiMergWay[idx][checkingPoint[idx]];
			}
		}

		/* Add the smallest string to sort buffer which shall be wrote to output file when it is full */
		if(sortBufSize + smallestStr.size() <= chunkSize)
		{
			try
			{
				sortBuf.push_back(smallestStr);
				sortBufSize += smallestStr.size() + sizeof(string);
			}
			catch(std::bad_alloc& ba)
			{
				/* update the output file in case the allocation is fail */
				for(string str:sortBuf)
				{
					opFile << str << endl;
				}

				sortBuf.clear();
				sortBufSize = smallestStr.size() + sizeof(string);
				sortBuf.push_back(smallestStr);
			}
		}
		else
		{
			/* Write to end of output file in case sort buffer is full*/
			for(string str:sortBuf)
			{
				opFile << str << endl;
			}

			sortBuf.clear();
			sortBufSize = smallestStr.size() + sizeof(string);
			sortBuf.push_back(smallestStr);
		}

		/* refill merge buffer in case it is empty */
		if(multiMergWay[smlIdx].size() == checkingPoint[smlIdx] + 1)
		{
			fileMergeBuff(chunkFiles[smlIdx],multiMergWay[smlIdx], pendingStr[smlIdx], chunkSize);
			checkingPoint[smlIdx] = 0;
		}
		else
		{
			checkingPoint[smlIdx]++;
		}

		/* Clear empty chunk buffer */
		if(multiMergWay[smlIdx].size() == 0)
		{
			multiMergWay.erase(multiMergWay.begin() + smlIdx);
			chunkFiles[smlIdx].close();
			chunkFiles.erase(chunkFiles.begin() + smlIdx);
			checkingPoint.erase(checkingPoint.begin()+ smlIdx);
			pendingStr.erase(pendingStr.begin() + smlIdx);
		}
	}

	if(sortBuf.size() > 0)
	{
		/* Add remaining string */
		for(int idx = 0; idx < sortBuf.size() - 1; ++idx)
		{
			opFile << sortBuf[idx] << endl;
		}

		opFile << sortBuf[sortBuf.size() - 1];
	}
	
	opFile.close();
	return 0;
} 
