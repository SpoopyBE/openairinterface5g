/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
//#define SHOWCOMP 1

inline void computeLLR(double llr[1+nmax][Nmax], uint16_t row, uint16_t col, 
		       uint16_t offset, uint8_t approximation) __attribute__((always_inline));
inline void computeLLR(double llr[1+nmax][Nmax], uint16_t row, uint16_t col, 
		       uint16_t offset, uint8_t approximation) {

        double a;
        double b;
	double absA,absB;


#ifdef SHOWCOMP
	printf("computeLLR (%d,%d,%d,%d)\n",row,col,offset,i);
#endif
	a = llr[col + 1][row];   
	b = llr[col+1][row + offset];
	
	if (approximation) { //eq. (9)
	  absA = fabs(a);
	  absB = fabs(b);
	  llr[col][row] = copysign(1.0, a) * copysign(1.0, b) * fmin(absA, absB);
	} else { //eq. (8a)
	  llr[col][row] = log((exp(a + b) + 1) / (exp(a) + exp(b)));
	}
}


void updateLLR(decoder_list_t **dlist,uint8_t **llrU, uint8_t **bitU,
	       uint8_t listSize, uint16_t row, uint16_t col, uint16_t xlen, uint8_t ylen, uint8_t approximation) {
  uint16_t offset = (xlen/(1<<(ylen-col-1)));
  if (( (row) % (2*offset) ) >= offset ) {
    if (bitU[row-offset][col]==0) updateBit(dlist, bitU, listSize, (row-offset), col, xlen, ylen);
    if (llrU[row-offset][col+1]==0) updateLLR(dlist, llrU, bitU, listSize, (row-offset), (col+1), xlen, ylen, approximation);
    if (llrU[row][col+1]==0) updateLLR(dlist, llrU, bitU, listSize, row, (col+1), xlen, ylen, approximation);
    for (uint8_t i=0; i<listSize; i++) {
      
      
#ifdef SHOWCOMP
      printf("updating LLR (%d,%d,%d) (bit %d,%d,%d, llr0 %d,%d,%d, llr1 %d,%d,%d \n",row,col,i,
	     row-offset,col,i,row-offset,col+1,i,row,col+1,i);
#endif
      dlist[i]->llr[col][row] = (pow((-1),dlist[i]->bit[col][row-offset])*dlist[i]->llr[col+1][row-offset]) + dlist[i]->llr[col+1][row];
    }
  } else {
    if (llrU[row][col+1]==0) updateLLR(dlist, llrU, bitU, listSize, row, (col+1), xlen, ylen, approximation);
    if (llrU[row+offset][col+1]==0) updateLLR(dlist, llrU, bitU, listSize, (row+offset), (col+1), xlen, ylen, approximation);
    for (int i=0;i<listSize;i++) computeLLR(dlist[i]->llr, row, col, offset, approximation);
    
  }
  
  llrU[row][col]=1;
}

void updateBit(decoder_list_t **dlist, uint8_t **bitU, uint8_t listSize, uint16_t row,
	       uint16_t col, uint16_t xlen, uint8_t ylen) {
  uint16_t offset = ( xlen/(pow(2,(ylen-col))) );
  
  for (uint8_t i=0; i<listSize; i++) {
    if (( (row) % (2*offset) ) >= offset ) {
      if (bitU[row][col-1]==0) updateBit(dlist, bitU, listSize, row, (col-1), xlen, ylen);
      dlist[i]->bit[col][row] = dlist[i]->bit[col-1][row];
#ifdef SHOWCOMP
      printf("updating bit (%d,%d,%d) from (%d,%d,%d)\n",
	     row,col,i,row,col-1,i);
#endif
    } else {
      if (bitU[row][col-1]==0) updateBit(dlist, bitU, listSize, row, (col-1), xlen, ylen);
      if (bitU[row+offset][col-1]==0) updateBit(dlist, bitU, listSize, (row+offset), (col-1), xlen, ylen);
      dlist[i]->bit[col][row] = ( (dlist[i]->bit[col-1][row]+dlist[i]->bit[col-1][row+offset]) % 2);
#ifdef SHOWCOMP
      printf("updating bit (%d,%d,%d) from (%d,%d,%d)+(%d,%d,%d)\n",
	     row,col,i,row,col-1,i,row+offset,col-1,i);
#endif
    }
  }
  
  bitU[row][col]=1;
}

void updatePathMetric(decoder_list_t **dlist,uint8_t listSize, uint8_t bitValue,
		uint16_t row, uint8_t approximation) {

#ifdef SHOWCOMP
  printf("updating path_metric from Frozen bit (%d,%d) \n",
	 row,0);
#endif

	if (approximation) { //eq. (12)
		for (uint8_t i=0; i<listSize; i++) {
			if ((2*bitValue) != ( 1 - copysign(1.0,dlist[i]->llr[0][row]) )) dlist[i]->pathMetric += fabs(dlist[i]->llr[0][row]);
		}
	} else { //eq. (11b)
		int8_t multiplier = (2*bitValue) - 1;
		for (uint8_t i=0; i<listSize; i++) dlist[i]->pathMetric += log ( 1 + exp(multiplier*dlist[i]->llr[0][row]) ) ;
	}

}

/*
void updatePathMetric2(double *pathMetric, double ***llr, uint8_t listSize, uint16_t row, uint8_t appr) {

	double *tempPM = malloc(sizeof(double) * listSize);
	for (int i=0; i < listSize; i++) tempPM[i]=pathMetric[i];

	uint8_t bitValue = 0;
	if (appr) { //eq. (12)
		for (uint8_t i = 0; i < listSize; i++) {
			if ((2 * bitValue) != (1 - copysign(1.0, llr[row][0][i]))) pathMetric[i] += fabs(llr[row][0][i]);
		}
	} else { //eq. (11b)
		int8_t multiplier = (2 * bitValue) - 1;
		for (uint8_t i = 0; i < listSize; i++) pathMetric[i] += log(1 + exp(multiplier * llr[row][0][i]));
	}

	bitValue = 1;
	if (appr) { //eq. (12)
		for (uint8_t i = listSize; i < 2*listSize; i++) {
			if ((2 * bitValue) != (1 - copysign(1.0, llr[row][0][(i-listSize)]))) pathMetric[i] = tempPM[(i-listSize)] + fabs(llr[row][0][(i-listSize)]);
		}
	} else { //eq. (11b)
		int8_t multiplier = (2 * bitValue) - 1;
		for (uint8_t i = listSize; i < 2*listSize; i++) pathMetric[i] = tempPM[(i-listSize)] + log(1 + exp(multiplier * llr[row][0][(i-listSize)]));
	}

	free(tempPM);

}
*/

void updatePathMetric2(decoder_list_t **dlist, uint8_t listSize, uint16_t row, uint8_t appr) {

  int i;

  for (i=0;i<listSize;i++) dlist[i+listSize]->pathMetric = dlist[i]->pathMetric;
  decoder_list_t **dlist2 = &dlist[listSize];

#ifdef SHOWCOMP
  printf("updating path_metric from information bit (%d,%d) \n",
	 row,0);
#endif  
  if (appr) { //eq. (12)
    for (i = 0; i < listSize; i++) {
      // bitValue=0
      if (dlist[i]->llr[0][row]<0) dlist[i]->pathMetric  -= dlist[i]->llr[0][row];
       // bitValue=1
      else                         dlist2[i]->pathMetric += dlist[i]->llr[0][row];
    }
  } else { //eq. (11b)
    for (i = 0; i < listSize; i++) {
      // bitValue=0
       dlist[i]->pathMetric += log(1 + exp(-dlist[i]->llr[0][row]));
      // bitValue=1
       dlist2[i]->pathMetric += log(1 + exp(dlist[i]->llr[0][row]));
    }
  }
}



void updateCrcChecksum(decoder_list_t **dlist, uint8_t **crcGen,
		       uint8_t listSize, uint32_t i2, uint8_t len) {
  for (uint8_t i = 0; i < listSize; i++) {
    for (uint8_t j = 0; j < len; j++) {
      dlist[i]->crcChecksum[j] = ( (dlist[i]->crcChecksum[j] + crcGen[i2][j]) % 2 );
    }
  }
}

void updateCrcChecksum2(decoder_list_t **dlist, uint8_t **crcGen,
			uint8_t listSize, uint32_t i2, uint8_t len) {
  for (uint8_t i = 0; i < listSize; i++) {
    for (uint8_t j = 0; j < len; j++) {
      dlist[i+listSize]->crcChecksum[j] = ( (dlist[i]->crcChecksum[j] + crcGen[i2][j]) % 2 );
    }
  }
}
