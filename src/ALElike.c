/*
 * Copyright (C) 2010,2011,2012 Scott Clark. All rights reserved.
 *
 * Developed by:
 * Scott Clark
 * Cornell University Center for Applied Mathematics
 * http://cam.cornell.edu
 * AND
 * Rob Egan
 * Department of Energy Joint Genome Institute
 * http://jgi.doe.gov
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal with the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 *   1. Redistributions of source code must retain the above copyright notice, 
 *      this list of conditions and the following disclaimers.
 *   2. Redistributions in binary form must reproduce the above copyright 
 *      notice, this list of conditions and the following disclaimers in the 
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of Cornell University, The Joint Genome Institute, 
 *      nor the names of its contributors may be used to endorse or promote 
 *      products derived from this Software without specific prior written 
 *      permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS WITH THE SOFTWARE.
 */

// For more information on the license please see 
// The University of Illinois/NCSA Open Source License
// http://www.opensource.org/licenses/UoI-NCSA.php

// ALElike.c

#include "ALElike.h"
#include "samtools_helper.h"

// casts a single numeric char to its int
int hackedIntCast(char c){
  if(c == '0'){return 0;}
  if(c == '1'){return 1;}
  if(c == '2'){return 2;}
  if(c == '3'){return 3;}
  if(c == '4'){return 4;}
  if(c == '5'){return 5;}
  if(c == '6'){return 6;}
  if(c == '7'){return 7;}
  if(c == '8'){return 8;}
  if(c == '9'){return 9;}
  return 0;
}

//uses Stirlings approximation to high precision
// http://en.wikipedia.org/wiki/Stirling's_approximation
// http://www.rskey.org/CMS/index.php/the-library/11
// lnfactconst2 = ln(2pi)/2
// this is log gamma by stirlings approx
double lnfact2(double input){
  double input2 = input*input;
  double input3 = input*input2;
  double input5 = input3*input2;
  double input7 = input5*input2;
  return (input - 0.5)*log(input) - input + lnfactconst2 + 1.0/(12.0*input) - 1.0/(360.0*input3) + 1.0/(1260.0*input5) - 1.0/(1680.0*input7);
}

double getNegBinomZnorm(double r){
  double ans = 0.0;
  int n = 0;
  double diff = 1.0;
  while((diff > 1e-8 || n < r) && n < 10000000){
      diff = exp(2.0*lnfact2(r + (double)n) - 2.0*lnfact2(r) - 2.0*lnfact2((double)n + 1.0) + (r + (double)n)*log(4.0));
      ans += diff;
      n++;
  }
  return ans;
}

// finds the log poisson pmf at value k for mean lambda
double poissonPMF(double k, double lambda){
  return k*log(lambda) - lambda - lnfact2(k + 1);
}

// finds the insert probability (assuming normal distribution) P(point | N(0,sigma))
double GetInsertProbNormal(double point, const double sigma){
  if (sigma == 0.0){
      return 1.0;
  }
  double p1 = erf((point + 0.5)/sqrt(2*sigma*sigma));
  double p2 = erf((point - 0.5)/sqrt(2*sigma*sigma));
  double prob = 0.5*(p1 - p2);
  ////printf("Point: %lf, p1: %lf, p2: %lf = %lf\n", point, erf((point + 0.5)/sqrt(2*sigma*sigma)), erf((point - 0.5)/sqrt(2*sigma*sigma)), prob);
  return prob;
}

double GetCappedInsertProbNormal(double maxSigma, double sigma) {
  return GetInsertProbNormal(maxSigma * sigma, sigma);
}

char *getMD(bam1_t *bam, char *ref) {
    char *md = (char*) bam_aux_get(bam, "MD");
    if (md == NULL) {
        bam_fillmd1_core(bam, ref);
        md = (char*) bam_aux_get(bam, "MD");
    }
    return md;
}

double getInsertLikelihoodBAM(bam1_t *read1, double mu, double sigma){
  assert(read1 != NULL);
  double mapLength = (double)getFragmentMapLenBAM(read1);
  assert(mapLength > 0.0);
  double likelihood = GetInsertProbNormal(fabs(mapLength - mu), sigma);
  //printf("getInsertLikelihoodBAM(%lf,%lf,%lf): %e\n",fabs(mapLength - mu), mu, sigma,likelihood);
  assert(likelihood >= 0.0 && likelihood <= 1.0);
  return likelihood;
}

// finds the loglikelihood of a string of misses in read from seqPos to matchLen
double loglikeMiss(char *readQual, int seqPos, int missLen, int qOff){
  int i;
  double loglikelihood = 0.0;
  for(i = seqPos; i < seqPos + missLen; i++){
    //likelihood = likelihood*((1.0 - 0.99)/3.0);
    //likelihood = likelihood*((1.0 - getQtoP(readQual[i], qOff))/3.0); // sometimes want -33/64?
    double logp = getQtoLogPMiss(readQual[i], qOff);
    ////printf("likeMiss(%d, %d, %c): %lf %lf %lf %lf\n", missLen, i, readQual[i] + 33, logp, loglikelihood, exp(logp), exp(loglikelihood));
    loglikelihood += logp;
  }
  ////printf("loglikeMiss(%d): %e\n", missLen, loglikelihood);
  assert(loglikelihood <= 0.0);
  return loglikelihood;
}

// finds the loglikelihood of a string of matches in read from seqPos to matchLen
double loglikeMatch(char *readQual, int seqPos, int matchLen, int qOff){
  int i;
  double loglikelihood = 0.0;
  for(i = seqPos; i < seqPos + matchLen; i++){
    loglikelihood += getQtoLogP(readQual[i], qOff);// sometimes want -33/64?
    ////printf("likeMatch %d %d %f %f %f\n", i, readQual[i], QtoLogP[readQual[i] - qOff], getQtoLogP(readQual[i], qOff), loglikelihood);
  }
  ////printf("loglikeMatch(%d): %e\n", matchLen, loglikelihood);
  assert(loglikelihood <= 0.0);
  return loglikelihood;
}

// finds the loglikelihood of an insertion (right now it is the same as a miss)
double loglikeInsertion(char *readQual, int seqPos, int insertionLength, int qOff) {
  // assume as unlikely as a substitution
  // TODO refine
  double loglikelihood = loglikeMiss(readQual, seqPos, insertionLength, qOff);
  ////printf("loglikeInsertion(%d): %e\n", insertionLength, loglikelihood);
  return loglikelihood;
}

// finds the loglikelihood of an deletion (right now it is the same as a miss)
double loglikeDeletion(char *readQual, int seqPos, int deletionLength, int qOff) {
  // assume as unlikely as a substitution of previous base
  // TODO refine
  int delPos = (seqPos > 0) ? seqPos - 1 : seqPos;
  assert(delPos >= 0);
  double loglikelihood = loglikeMiss(readQual, delPos, 1, qOff) * (double)deletionLength;
  ////printf("loglikeDeletion(%d): %e\n", deletionLength, loglikelihood);
  return loglikelihood;
}

int getBaseAmbibuity (char c) {
   int ambiguity = 0;
   switch(c) {
       // a real base
       case 'A':
       case 'T':
       case 'C':
       case 'G': ambiguity = 1; break;
       // an ambiguity base
       case 'N': ambiguity = 4; break;
       case 'M':
       case 'R':
       case 'W':
       case 'S':
       case 'Y':
       case 'K': ambiguity = 2; break;
       case 'V':
       case 'H':
       case 'D':
       case 'B': ambiguity = 3; break;
       default: break;
   }
   return ambiguity;
}

/*
double getMDLogLikelihoodAtPosition(char *MD, char *readQual, int qOff, int position) {
  assert(MD != NULL && MD[0] != '\0');
  assert(readQual != NULL);
  //assert(readQual[0] != '\0');

  int stop = 0;
  int pos = 0;
  int seqPos = 0;
  double loglikelihood = 0.0;
  double loglikelihoodAtPosition = 0.0;

  // parse MD field
  while(stop == 0){
    // matches
    int seqCount = 0;
    while(isdigit(MD[pos])){
          seqCount = seqCount*10 + (int)(MD[pos]) - 48; // chr(48) == '0'
        pos++;
    }
    seqPos += seqCount;
    double logMatch;
    double logMiss;
    // misses
    int baseAmbiguity = 0;
    while((baseAmbiguity = getBaseAmbibuity(MD[pos])) > 0){
      logMatch = loglikeMatch(readQual, seqPos, 1, qOff);
      if(baseAmbiguity == 1){
          logMiss = loglikeMiss(readQual, seqPos, 1, qOff);
      } else {
          logMiss = log(1.0/(float)baseAmbiguity); // TODO adjust for ambiguity scale (coding for 2, 3 or 4 bases)
      }
      loglikelihood += logMiss - logMatch;
      if(position == seqPos){
          loglikelihoodAtPosition = logMiss - logMatch;
      }
      seqPos++;
      pos++;
      ////printf("MD %d miss  %d. %f\n", seqPos, 1, loglikelihood);
    }

    // deletions
    if(MD[pos] == '^'){
      pos++;
      while(isalpha(MD[pos])){
        pos++;
      }
    }

    // sees if we are at the end
    if(MD[pos] == '\0'){
      stop = 1;
      continue;
    }
  }

  ////printf("getMDLogLikelihood(%s): %e\n", MD, loglikelihood);
  // no assertion that logLikelihood is <= 0, as this is a correction to the logMatch already applied
  return loglikelihoodAtPosition;
}
*/

/*
// used to reduce loglikelihood in case of missmatches only
// (CIGAR already has accounted for matchlength, inserts, deletions)
double getMDLogLikelihood(char *MD, char *readQual, int qOff) {
  assert(MD != NULL && MD[0] != '\0');
  assert(readQual != NULL);
  //assert(readQual[0] != '\0');

  int stop = 0;
  int pos = 0;
  int seqPos = 0;
  double loglikelihood = 0.0;

  // parse MD field
  while(stop == 0){
    // matches
    int seqCount = 0;
    while(isdigit(MD[pos])){
          seqCount = seqCount*10 + (int)(MD[pos]) - 48; // chr(48) == '0'
        pos++;
    }
    seqPos += seqCount;
    double logMatch;
    double logMiss;
    // misses
    int baseAmbiguity = 0;
    while((baseAmbiguity = getBaseAmbibuity(MD[pos])) > 0){
      logMatch = loglikeMatch(readQual, seqPos, 1, qOff);
      if (baseAmbiguity == 1) {
    	  logMiss = loglikeMiss(readQual, seqPos, 1, qOff);
      } else {
          logMiss = log(1.0/(float)baseAmbiguity); // TODO adjust for ambiguity scale (coding for 2, 3 or 4 bases)
      }
      loglikelihood += logMiss - logMatch;
      seqPos++;
      pos++;
      ////printf("MD %d miss  %d. %f\n", seqPos, 1, loglikelihood);
    }

    // deletions
    if(MD[pos] == '^'){
      pos++;
      while(isalpha(MD[pos])){
        pos++;
      }
    }

    // sees if we are at the end
    if(MD[pos] == '\0'){
      stop = 1;
      continue;
    }
  }

  ////printf("getMDLogLikelihood(%s): %e\n", MD, loglikelihood);
  // no assertion that logLikelihood is <= 0, as this is a correction to the logMatch already applied
  return loglikelihood;
}
*/

/*
double getCIGARLogLikelihoodAtPosition(int numCigarOperations, uint32_t *cigar, char *readQual, int qOff, int *inserts, int *deletions, int *totalMatch, int position) {
  int i;
  int seqPos = 0;
  double logLikelihood = 0.0;
  double logLikelihoodAtPosition = 0.0;
  for(i=0 ; i < numCigarOperations ; i++) {
    uint32_t cigarInt = *(cigar+i);
    uint32_t cigarFlag = (cigarInt & BAM_CIGAR_MASK);
    uint32_t count = (cigarInt >> BAM_CIGAR_SHIFT);
    ////printf("CIGAR: cigInt: %u cigFlag: %u count: %u pos: %d of %u\n", cigarInt, cigarFlag, count, position, seqPos);
    switch (cigarFlag) {
      case(BAM_CMATCH) :
        *totalMatch += count;
        //likelihood *= likeMatch(readQual, seqPos, count, qOff);
        logLikelihood += loglikeMatch(readQual, seqPos, count, qOff);
        if(seqPos <= position && position <= seqPos + count){
          logLikelihoodAtPosition = loglikeMatch(readQual, seqPos, 1, qOff);
        }
        seqPos += count;
        break;
      case(BAM_CINS)   :
        *inserts += count;
        //likelihood *= likeInsertion(readQual, seqPos, count, qOff);
        logLikelihood += loglikeInsertion(readQual, seqPos, count, qOff);
        if(seqPos <= position && position <= seqPos + count){
          logLikelihoodAtPosition = loglikeInsertion(readQual, seqPos, 1, qOff);
        }
        seqPos += count;
        break;
      case(BAM_CPAD)   :
        *inserts += count;
        //likelihood *= likeInsertion(readQual, seqPos, count, qOff);
        logLikelihood += loglikeInsertion(readQual, seqPos, count, qOff);
        if(seqPos <= position && position <= seqPos + count){
          logLikelihoodAtPosition = loglikeInsertion(readQual, seqPos, 1, qOff);
        }
        seqPos += count;
        break;
      case(BAM_CDEL)   :
        *deletions += count;
        //likelihood *= likeDeletion(readQual, seqPos, count, qOff);
        logLikelihood += loglikeDeletion(readQual, seqPos, count, qOff);
        if(seqPos <= position && position <= seqPos + count){
          logLikelihoodAtPosition = loglikeDeletion(readQual, seqPos, 1, qOff);
        }
        // deletions do not increase seqPos
        break;
      case(BAM_CREF_SKIP):
        if(seqPos <= position && position <= seqPos + count){
          logLikelihoodAtPosition = loglikeInsertion(readQual, seqPos, 1, qOff);
        }
        // assume this is a spliced alignment for RNA, so okay
        break;
      case(BAM_CHARD_CLIP):
        // hard clipped region is not represented in sequence or quality string, so no information is available for analysis
        break;
      case(BAM_CSOFT_CLIP):
        //likelihood *= likeMiss(readQual, seqPos, count, qOff);
        logLikelihood += loglikeMiss(readQual, seqPos, count, qOff);
        if(seqPos <= position && position <= seqPos + count){
          logLikelihoodAtPosition = loglikeMiss(readQual, seqPos, 1, qOff);
        }
        seqPos += count;
        break;
    }
  }
  //double likelihood = exp(logLikelihood);
  ////printf("getCIGARLikelihoodBAM(): %lf\n", logLikelihoodAtPosition);
  if(logLikelihoodAtPosition == 0.0){ // reached end, assume it is a deletion
    logLikelihoodAtPosition = loglikeDeletion(readQual, seqPos, 1, qOff);
  }
  return logLikelihoodAtPosition;
}
*/

void getContributionsForPositions(bam1_t *read, char *contigSeq, int qOff, int alignmentLength, float *depthPositions, double *loglikelihoodPositions) {
  int i,j;
  int seqPos = 0;
  int refPos = 0;

  if(read == NULL){
	for(refPos = 0; refPos < alignmentLength; refPos++) {
		depthPositions[refPos] = 0.0;
		loglikelihoodPositions[refPos] = getMinLogLike();
	}
	return;
  }

  // read CIGAR first
  uint32_t *cigar = bam1_cigar(read);
  char *readQual = (char*) bam1_qual(read);
  char *MD = getMD(read, contigSeq);
  int numCigarOperations = read->core.n_cigar;
  int ref2seqPos[alignmentLength];

  // initialize loglikelihoodPositions
  for(j=0; j < alignmentLength; j++) {
    loglikelihoodPositions[j] = 0.0;
  }

  for(i=0 ; i < numCigarOperations ; i++) {
    uint32_t cigarInt = *(cigar+i);
    uint32_t cigarFlag = (cigarInt & BAM_CIGAR_MASK);
    uint32_t count = (cigarInt >> BAM_CIGAR_SHIFT);
    ////printf("CIGAR: %u %u %u\n", cigarInt, cigarFlag, count);
    switch (cigarFlag) {
      case(BAM_CMATCH): // match or mismatch
      case(BAM_CEQUAL): // match
      case(BAM_CDIFF):  // mismatch
        for(j=0; j < count; j++) {
        	loglikelihoodPositions[refPos+j] += loglikeMatch(readQual, seqPos+j, 1, qOff);
        	depthPositions[refPos+j] = 1.0;
		ref2seqPos[refPos+j] = seqPos+j;
        }
        seqPos += count;
        refPos += count;
        break;
      case(BAM_CINS)   : // insertion to the reference.
		loglikelihoodPositions[refPos] += loglikeInsertion(readQual, seqPos, count, qOff);
        // only increment seqPos
        seqPos += count;
        break;
      case(BAM_CDEL): // deletion from the reference
		for(j=refPos; j < refPos+count; j++) {
			loglikelihoodPositions[j] += loglikeDeletion(readQual, seqPos, count, qOff) / count;
			depthPositions[j] = 0.0;
			ref2seqPos[j] = seqPos;
		}
        // only increment refPos
        refPos += count;
        break;
      case(BAM_CREF_SKIP):
        // assume this is a spliced alignment for RNA, so okay, but no data
        // only increment refPos
		for(j=refPos; j < refPos+count; j++) {
			depthPositions[j] = 0.0;
			ref2seqPos[j] = seqPos;
		}
		refPos += count;
        break;
      case(BAM_CPAD): // silent deletion from padded reference (not in reference or query)
      case(BAM_CHARD_CLIP):
		// hard clipped region is not represented in sequence or quality string, so no information is available for analysis
        break;
      case(BAM_CSOFT_CLIP):
        // soft clipped sequences present in SEQ
        seqPos += count;
        break;
    }
  }
  assert(seqPos == read->core.l_qseq);
  assert(refPos == alignmentLength);

  // now process MD field
  refPos = 0;
  if (MD != NULL && MD[0] == 'Z') {
	  int stop = 0;
	  int pos = 1;

	  // parse MD field
	  while(stop == 0){
	    // matches
	    int seqCount = 0;
	    while(isdigit(MD[pos])){
	        seqCount = seqCount*10 + (int)(MD[pos]) - 48; // chr(48) == '0'
	        pos++;
	    }
	    refPos += seqCount;

	    double logMatch;
	    double logMiss;
	    // misses
	    int baseAmbiguity = 0;
	    while((baseAmbiguity = getBaseAmbibuity(MD[pos])) > 0){
              seqPos = ref2seqPos[refPos];
	      logMatch = loglikeMatch(readQual, seqPos, 1, qOff);
	      if(baseAmbiguity == 1){
	          logMiss = loglikeMiss(readQual, seqPos, 1, qOff);
	      } else {
	          logMiss = log(1.0/(float)baseAmbiguity);
	      }
	      loglikelihoodPositions[refPos] += logMiss - logMatch;

	      refPos++;
	      pos++;
	      ////printf("MD %d miss  %d. %f\n", seqPos, 1, loglikelihood);
	    }

	    // deletions
	    if(MD[pos] == '^'){
	      pos++;
	      while(isalpha(MD[pos])){
	        pos++;
	        refPos++;
	      }
	    }

	    // sees if we are at the end
	    if(MD[pos] == '\0'){
	      stop = 1;
	      continue;
	    }
	  }
	  if (refPos != alignmentLength) fprintf(stderr, "WARNING: CIGAR and MD field disagree on match region for %s\n", bam1_qname(read));
  } else {
	  printf("WARNING: could not find the MD tag for %s\n", bam1_qname(read));
  }
}

/*
double getCIGARLogLikelihoodBAM(int numCigarOperations, uint32_t *cigar, char *readQual, int qOff, int *inserts, int *deletions, int *totalMatch) {
  int i;
  int seqPos = 0;
  double logLikelihood = 0.0;
  for(i=0 ; i < numCigarOperations ; i++) {
    uint32_t cigarInt = *(cigar+i);
    uint32_t cigarFlag = (cigarInt & BAM_CIGAR_MASK);
    uint32_t count = (cigarInt >> BAM_CIGAR_SHIFT);
    ////printf("CIGAR: %u %u %u\n", cigarInt, cigarFlag, count);
    switch (cigarFlag) {
      case(BAM_CMATCH) :
        *totalMatch += count;
        //likelihood *= likeMatch(readQual, seqPos, count, qOff);
        logLikelihood += loglikeMatch(readQual, seqPos, count, qOff);
        seqPos += count;
        break;
      case(BAM_CINS)   :
        *inserts += count;
        //likelihood *= likeInsertion(readQual, seqPos, count, qOff);
        logLikelihood += loglikeInsertion(readQual, seqPos, count, qOff);
        seqPos += count;
        break;
      case(BAM_CPAD)   :
        *inserts += count;
        //likelihood *= likeInsertion(readQual, seqPos, count, qOff);
        logLikelihood += loglikeInsertion(readQual, seqPos, count, qOff);
        seqPos += count;
        break;
      case(BAM_CDEL)   :
        *deletions += count;
        //likelihood *= likeDeletion(readQual, seqPos, count, qOff);
        logLikelihood += loglikeDeletion(readQual, seqPos, count, qOff);
        // deletions do not increase seqPos
        break;
      case(BAM_CREF_SKIP):
        // assume this is a spliced alignment for RNA, so okay
        logLikelihood += loglikeInsertion(readQual, seqPos, count, qOff);
        seqPos += count;
        break;
      case(BAM_CHARD_CLIP):
		// hard clipped region is not represented in sequence or quality string, so no information is available for analysis
        break;
      case(BAM_CSOFT_CLIP):
        //likelihood *= likeMiss(readQual, seqPos, count, qOff);
        logLikelihood += loglikeMiss(readQual, seqPos, count, qOff);
        seqPos += count;
        break;
    }
  }
  //double likelihood = exp(logLikelihood);
  ////printf("getCIGARLikelihoodBAM(): %e, %e\n", likelihood, logLikelihood);
  assert(logLikelihood <= 0.0);
  return logLikelihood;
}
*/

/*
double getMatchLogLikelihoodAtPosition(bam1_t *read, int qOff, int position, char *md){
  if(read == NULL){
    return getMinLogLike();
  }
  double loglikelihood;
  // read CIGAR first
  char *readQual = (char*) bam1_qual(read);
  uint32_t *cigar = bam1_cigar(read);
  int inserts = 0;
  int deletions = 0;
  int totalMatch = 0;
  ////printf("getMatchLikelihoodBAM(%s, %d)\n", bam1_qname(read), qOff);

  loglikelihood = getCIGARLogLikelihoodAtPosition(read->core.n_cigar, cigar, readQual, qOff, &inserts, &deletions, &totalMatch, position);
  assert(loglikelihood <= 0.0);

  ////printf("%s %f MD:%s\n", bam1_qname(read), likelihood, md);

  if (md != NULL && md[0] == 'Z') {
      loglikelihood += getMDLogLikelihoodAtPosition(md + 1, readQual, qOff, position);
  } else {
      printf("WARNING: could not find the MD tag for %s\n", bam1_qname(read));
  }

  ////printf("getMatchLogLikelihoodBAM(%s, %d) = %e\n", bam1_qname(read), qOff, loglikelihood);
  if(loglikelihood == 0.0){
      //printf("0.0 pos log");
  }
  return loglikelihood;
}
*/
/*
// takes in a read and returns the match loglikelihood (due to matches, mismatches, indels)
double getMatchLogLikelihoodBAM(bam1_t *read, int qOff, char *md){
  assert(read != NULL);
  double loglikelihood;

  // read CIGAR first
  char *readQual = (char*) bam1_qual(read);
  uint32_t *cigar = bam1_cigar(read);
  int inserts = 0;
  int deletions = 0;
  int totalMatch = 0;
  ////printf("getMatchLikelihoodBAM(%s, %d)\n", bam1_qname(read), qOff);

  loglikelihood = getCIGARLogLikelihoodBAM(read->core.n_cigar, cigar, readQual, qOff, &inserts, &deletions, &totalMatch);
  assert(loglikelihood <= 0.0);

  ////printf("%s %f MD:%s\n", bam1_qname(read), likelihood, md);
  if (md != NULL && md[0] == 'Z') {
      loglikelihood += getMDLogLikelihood(md + 1, readQual, qOff);
  } else {
     printf("WARNING: could not find the MD tag for %s\n", bam1_qname(read));
  }

  //printf("getMatchLogLikelihoodBAM(%s, %d) = %e\n", bam1_qname(read), qOff, loglikelihood);
  return loglikelihood;
}
*/

double getMatchLogLikelihoodBAM(bam1_t *read, char *contigSeq, int qOff, int alignmentLength) {
	float depthContributions[alignmentLength];
	double placeLogLikelihoods[alignmentLength];
	int i;
	getContributionsForPositions(read, contigSeq, qOff, alignmentLength, depthContributions, placeLogLikelihoods);

	double loglikelihood = 0.0;
	for(i = 0; i < alignmentLength ; i++)
		loglikelihood += placeLogLikelihoods[i];
	if (loglikelihood < getMinLogLike())
		loglikelihood = getMinLogLike();
	return loglikelihood;
}

// returns the 2-bit hash representation of a nucl. given its place in the kmer
int kmerHash(char c1, int place){
  int hash = 0;
  switch(c1) {
  case 'A': case 'a': break;
  case 'T': case 't': hash = 0x1 << (place*2); break;
  case 'C': case 'c': hash = 0x2 << (place*2); break;
  case 'G': case 'g': hash = 0x3 << (place*2); break;
  default: hash = -16777216;
  }
  return hash;
}

// builds up a 2*kmerLen-bit hash (for the seq) given a seqenence and starting position
int getKmerHash(char *seq, int startPos, int kmerLen){
  int i;
  int hash = 0;
  for(i = 0; i < kmerLen; i++){
    hash += kmerHash(seq[startPos+i], i);
  }
  return hash;
}

// calculates the kmer statistics
void computeKmerStats(assemblyT *theAssembly, int kmerLen){
  int i, j, k, hash;
  long totalKmers, totalKmersInit;
  totalKmers = 0;
  totalKmersInit = pow(4, kmerLen);
  // calculate total possible kmers
  long *kmerVec = malloc(totalKmersInit*sizeof(long));
  double kmerSum = 0.0;
  double kmerNorm = 0.0;
  double kmerZnorm = 0.0;

  if (!isMetagenome()) {
	    // initialize kmerVec
	    for(j = 0; j < totalKmersInit; j++){
	      kmerVec[j] = 0;
	    }
	    totalKmers = 0;
	    for(i = 0; i < theAssembly->numContigs; i++){
	    	contig_t *contig = theAssembly->contigs[i];
	    	if (contig->seqLen > kmerLen) {
	        	// add up the kmers
	        	for(j = 0; j < contig->seqLen - kmerLen; j++){
	        		hash = getKmerHash(contig->seq, j, kmerLen);
	        		////printf("Hash = %i\n", hash);
	        		if(hash > -1){
	        			kmerVec[hash]++;
	        			totalKmers++;
	        		}
	        	}
	    	}
	    }
	    for(i = 0; i < theAssembly->numContigs; i++){
	    	contig_t *contig = theAssembly->contigs[i];
	    	if (contig->seqLen > kmerLen) {
	    		for(j = 0; j < contig->seqLen - kmerLen; j++){
	    			hash = getKmerHash(contig->seq, j, kmerLen);
	    			if(hash > -1){
	    				double score = ((double)kmerVec[hash]) / ((double)totalKmers);
	    				double logScore = log(score);


	    				theAssembly->totalScore += logScore; // k-mer contribution to totalScore
	    				theAssembly->kmerAvgSum += logScore;
	    				theAssembly->kmerAvgNorm += 1.0;
	    				kmerSum += score;
	    				kmerNorm += 1.0;
	    			}
	    		}
	    	}
	    }
  }

  // find all kmers present
  for(i = 0; i < theAssembly->numContigs; i++){
    contig_t *contig = theAssembly->contigs[i];
    if (contig->seqLen <= kmerLen){
        for(j = 0; j < contig->seqLen; j++){
            contig->kmerLogLikelihood[j] = getMinLogLike();
        }
        theAssembly->totalScore += getMinLogLike(); // if the kmer is too short to make a kmer it gets low k-mer related totalScore
        theAssembly->kmerAvgSum += getMinLogLike();
        theAssembly->kmerAvgNorm += 1.0;
        continue;
    }

    if (isMetagenome()) {
    	// initialize kmerVec
    	for(j = 0; j < totalKmersInit; j++){
    		kmerVec[j] = 0;
    	}
    	totalKmers = 0;

    	// add up the kmers
    	for(j = 0; j < contig->seqLen - kmerLen; j++){
    		hash = getKmerHash(contig->seq, j, kmerLen);
    		////printf("Hash = %i\n", hash);
    		if(hash > -1){
    			kmerVec[hash]++;
    			totalKmers++;
    		}
    	}

    	kmerSum = 0.0;
    	kmerNorm = 0.0;

    	// apply the kmer score to the total score
    	for(j = 0; j < contig->seqLen - kmerLen; j++){
    		hash = getKmerHash(contig->seq, j, kmerLen);
    		if(hash > -1){
    			double score = ((double)kmerVec[hash]) / ((double)totalKmers);
    			double logScore = log(score);
    			theAssembly->totalScore += logScore; // k-mer contribution to totalScore
    			theAssembly->kmerAvgSum += logScore;
    			theAssembly->kmerAvgNorm += 1.0;
    			kmerSum += score;
    			kmerNorm += 1.0;
    		}
    	}
    }

    kmerZnorm = log(kmerSum/kmerNorm);
   	// z normalize
   	if(contig->seqLen - kmerLen > 0){
   		theAssembly->totalScore -= kmerZnorm*(double)(contig->seqLen - kmerLen);
   		theAssembly->kmerAvgSum -= kmerZnorm*(double)(contig->seqLen - kmerLen);
   	}

    //
    // ** Using contig->kmerLogLikelihood[] as just a likelihood.  Will convert back to log(likelihood) at the end **
    //

    ////printf("Calculated all %i kmers!\n", totalKmers);
    // calculate probability of seeing that kmer based on the rest of the contig
    // first kmer - 1 unrolled
    for(j = 0; j < kmerLen; j++){
      contig->kmerLogLikelihood[j] = 0.0;
      for(k = 0; k < j+1; k++){
        hash = getKmerHash(contig->seq, k, kmerLen);
        if(hash > -1){
          contig->kmerLogLikelihood[j] += 1.0/(double)(j+1)*(double)(kmerVec[hash])/(double)(totalKmers);
        }
      }
      ////printf("New likelihood[%i]: %f.\n", j, contig->kmerLikelihood[j]);
    }
    ////printf("First.\n");
    // middle bunch
    for(j = kmerLen; j < contig->seqLen - kmerLen; j++){
      contig->kmerLogLikelihood[j] = 0.0;
      for(k = 0; k < kmerLen; k++){
        hash = getKmerHash(contig->seq, j - k, kmerLen);
        if(hash > -1){
          contig->kmerLogLikelihood[j] += 1.0/(double)(kmerLen)*(double)(kmerVec[hash])/(double)(totalKmers);
        }
      }
      ////printf("New likelihood[%i]: %f.\n", j, contig->kmerLikelihood[j]);
    }
    ////printf("Mid.\n");
    // last bits
    for(j = contig->seqLen - kmerLen; j < contig->seqLen; j++){
      contig->kmerLogLikelihood[j] = 0.0;
      for(k = j - kmerLen; k < j - kmerLen + (contig->seqLen - j); k++){
        hash = getKmerHash(contig->seq, k, kmerLen);
        if(hash > -1){
          contig->kmerLogLikelihood[j] += 1.0/(double)(contig->seqLen - j)*(double)(kmerVec[hash])/(double)(totalKmers);
        }
      }
      ////printf("New likelihood[%i]: %f.\n", j, contig->kmerLikelihood[j]);
    }
    ////printf("Last.\n");
    totalKmers = 0;

    //
    // ** Converting contig->kmerLogLikelihood[] back to log(likelihood) and normalizing **
    //

    // add up kmer score into total score
    for(j = 0; j < contig->seqLen; j++){
      //assert(contig->kmerLikelihood[j] <= 1.0);
      if(log(contig->kmerLogLikelihood[j]) - kmerZnorm < getMinLogLike()){
        contig->kmerLogLikelihood[j] = getMinLogLike();
      }else{
        contig->kmerLogLikelihood[j] = log(contig->kmerLogLikelihood[j]) - kmerZnorm;
      }
    }
  }
  free(kmerVec);
}

// used in getPlacementWinner
unsigned int JSHash(char* str)
{
  unsigned int hash = 1315423911;
  char c;

  while (str != NULL && (c=*str++) != '\0')
  {
    hash ^= ((hash << 5) + c + (hash >> 2));
  }

  return hash;
}

// finds the sum of the total likelihoods given the head of the list
double getTotalLikelihood(alignSet_t *head) {
  double likeNormalizer = 0.0;
  likeNormalizer += head->likelihood*head->likelihoodInsert;
  alignSet_t *current = head;
  while(current->nextAlignment != NULL){
    current = current->nextAlignment;
    likeNormalizer += current->likelihood*current->likelihoodInsert;
  }
  return likeNormalizer;
}

// find the placement winner
alignSet_t *getPlacementWinner(alignSet_t *head, double likeNormalizer, int *winner) {
  assert(head != NULL && winner != NULL);
  assert(head->likelihood >= 0.0);

  *winner = -1;
  if(likeNormalizer == 0.0){ // no real placement
    return NULL;
  }

  // instead of purely random choice, choose a consistent, but still random choice
  unsigned int iseed = JSHash(head->name);
  srand(iseed); // instead of: srand ((unsigned int) time(NULL));

  double tRand = ( likeNormalizer * rand() / ( RAND_MAX + 1.0 ) );
  double soFar = 0.0;

  int i = 0;
  alignSet_t *current = head;
  if(head->likelihood*head->likelihoodInsert > tRand){
    *winner = 0;
  }else{
    assert(head->likelihood >= 0.0);
    soFar += head->likelihood*head->likelihoodInsert;
    while(current->nextAlignment != NULL){
      current = current->nextAlignment;
      assert(current->likelihood >= 0.0);
      i++;
      if(current->likelihood*current->likelihoodInsert + soFar > tRand){
        *winner = i;
        break;
      }else{
        soFar += current->likelihood*current->likelihoodInsert;
      }
    }
  }
  assert(current->likelihood >= 0.0);
  if(current->likelihood*current->likelihoodInsert > 0.0){
    return current;
  }else{ // not a real placement
    return NULL;
  }
}


// apply statistics
int applyDepthAndMatchToContig(alignSet_t *alignment, assemblyT *theAssembly, double likeNormalizer, libraryParametersT *libParams) {
  assert(alignment->likelihood >= 0.0);
  double logLikelihood = log(alignment->likelihood);
  double logLikelihoodInsert = log(alignment->likelihoodInsert);
  double tmpLogLike = 0.0;
  double totalPositionPlaceLikelihood = 0.0;
  int j, i;
  int numberMapped = 0;
  double depthContribution;
  enum MATE_ORIENTATION orientation = alignment->orientation;
  double orientationLogLikelihood = 0.0;

  switch(orientation) {
  case(READ1_ONLY):
  case(READ2_ONLY):
  case(SINGLE_READ):
  	  orientationLogLikelihood = log(libParams->totalValidSingleFraction); break;
  case(CHIMER):
  	  orientationLogLikelihood = log(libParams->totalChimerMateFraction); break;
  default:
	  orientationLogLikelihood = log(libParams->mateParameters[orientation].libraryFraction);
	  break;
  }
  double logLikelihoodPlacement = 0.0;

  // READ 1
  if (alignment->start1 >= 0 && alignment->end1 >= 0 && alignment->contigId1 >= 0 && alignment->contigId1 < theAssembly->numContigs) {
    numberMapped += 1;
    contig_t *contig1 = theAssembly->contigs[alignment->contigId1];
    assert(alignment->start1 < contig1->seqLen);
    assert(alignment->end1 <= contig1->seqLen);
    assert(alignment->start1 < alignment->end1);
    int alignmentLength = alignment->end1 - alignment->start1;
    float depthContributions[alignmentLength];
    double placeLogLikelihoods[alignmentLength];
    getContributionsForPositions(alignment->bamOfAlignment1, contig1->seq, libParams->qOff, alignmentLength, depthContributions, placeLogLikelihoods);
    i = 0;
    for(j = alignment->start1; j < alignment->end1; j++){
      // DEPTH SCORE
      // discount indels
      depthContribution = depthContributions[i]; //getDepthContributionAtPositionBAM(alignment->bamOfAlignment1, qOff, i);
      contig1->depth[j] += depthContribution;
      theAssembly->overlapAvgSum += depthContribution;      
      //contig1->depth[j] += 1.0; // We picked a winner, it gets full prob

      // PLACEMENT SCORE
      // TODO make it BAM dependent
      // BAMv2 version
      logLikelihoodPlacement = orientationLogLikelihood + placeLogLikelihoods[i]; //exp(getMatchLogLikelihoodAtPosition(alignment->bamOfAlignment1, qOff, i, md1)); // + log(alignment->likelihoodInsert);
      i++;
      // old way
      if(logLikelihoodPlacement > getMinLogLike() && !isnan(logLikelihoodPlacement)){
        contig1->matchLogLikelihood[j] += logLikelihoodPlacement;
      }else{
        contig1->matchLogLikelihood[j] += getMinLogLike();
      }

      // LIKELIHOOD SCORE
      if(logLikelihoodInsert > getMinLogLike() && !isnan(logLikelihoodInsert)){
        contig1->insertLogLikelihood[j] += logLikelihoodInsert;
      }else{
        contig1->insertLogLikelihood[j] += getMinLogLike();
      }
    }
    theAssembly->overlapAvgNorm += 1.0;
  }

  // READ 2
  // check for a valid read2 entry
  if (alignment->start2 >= 0 && alignment->end2 >= 0 && alignment->contigId2 >= 0 && alignment->contigId2 < theAssembly->numContigs && alignment->start2 != alignment->start1) {
    numberMapped += 1;
    contig_t *contig2 = theAssembly->contigs[alignment->contigId2];
    assert(alignment->start2 < contig2->seqLen);
    assert(alignment->end2 <= contig2->seqLen);
    assert(alignment->start2 < alignment->end2);
    assert(alignment->bamOfAlignment2 != NULL);
    int alignmentLength = alignment->end2 - alignment->start2;
    float depthContributions[alignmentLength];
    double placeLogLikelihoods[alignmentLength];
    getContributionsForPositions(alignment->bamOfAlignment2, contig2->seq, libParams->qOff, alignmentLength, depthContributions, placeLogLikelihoods);
    i = 0;
    for(j = alignment->start2; j < alignment->end2; j++){
      // DEPTH SCORE
      // discount indels
      depthContribution = depthContributions[i]; // getDepthContributionAtPositionBAM(alignment->bamOfAlignment2, qOff, i);
      contig2->depth[j] += depthContribution;
      theAssembly->overlapAvgSum += depthContribution;
      //contig2->depth[j] += 1.0;
      
      // PLACEMENT SCORE
      // TODO make it BAM dependent
      // BAMv2 version
      logLikelihoodPlacement = orientationLogLikelihood + placeLogLikelihoods[i]; // exp(getMatchLogLikelihoodAtPosition(alignment->bamOfAlignment2, qOff, i, md2)); // + log(alignment->likelihoodInsert);
      i++;
      // old way
      if(logLikelihoodPlacement > getMinLogLike() && !isnan(logLikelihoodPlacement)){
        contig2->matchLogLikelihood[j] += logLikelihoodPlacement;
      }else{
        contig2->matchLogLikelihood[j] += getMinLogLike();
      }

      // INSERT SCORE
      if(logLikelihoodInsert > getMinLogLike() && !isnan(logLikelihoodInsert)){
        contig2->insertLogLikelihood[j] += logLikelihoodInsert;
      }else{
        contig2->insertLogLikelihood[j] += getMinLogLike();
      }
    }
    theAssembly->overlapAvgNorm += 1.0;
  }
  // TOTAL SCORE
  // apply match likelihood to total likelihood
  if(numberMapped > 0){
      if(logLikelihood > getMinLogLike() && !isnan(logLikelihood)){
        tmpLogLike = logLikelihood;
      }else{
        tmpLogLike = getMinLogLike();
      }
      // mated reads and single reads both only hit the total score once
      theAssembly->totalScore += tmpLogLike; // match contribution to totalScore from placement
      theAssembly->placeAvgSum += tmpLogLike;
      theAssembly->placeAvgNorm += 1.0;      
  }
  
  // apply insert likelihood to total likelihood
  if(logLikelihoodInsert > getMinLogLike() && !isnan(logLikelihoodInsert)){
    tmpLogLike = logLikelihoodInsert;
  }else{
    tmpLogLike = getMinLogLike();
  }
  // this happens whether double, single or unmapped
  theAssembly->totalScore += tmpLogLike; // match contribution to totalScore from insert
  theAssembly->insertAvgSum += tmpLogLike;
  theAssembly->insertAvgNorm += 1.0;

  //printf("totalScore: %lf %lf\n", theAssembly->totalScore, theAssembly->insertAvgSum);
  return numberMapped;
}

// this applies the placement(s) to the assembly part (SINGLE PART)
// I feel like this could be sped up with a hash table vs the current linked lists, but we will see...
int applyPlacement(alignSet_t *head, assemblyT *theAssembly, libraryParametersT *libParams){

  assert(head != NULL);
  // normalize the probs
  double likeNormalizer = getTotalLikelihood(head); // sum of all likelihoods
  
  // finds where to place the read (if multiple mappings) if anywhere
  int winner = -1;
  alignSet_t *current = getPlacementWinner(head, likeNormalizer, &winner);

  if(current == NULL){ // no winner
	//fprintf(stderr, "applyPlacement(%s): unmapped\n", head->name);
    return 0; // 0 mapped
  }

  if (current->bamOfAlignment1 != 0)
	  assert(strcmp(head->name, bam1_qname(current->bamOfAlignment1)) == 0);
  if (current->bamOfAlignment2 != 0)
	  assert(strcmp(head->name, bam1_qname(current->bamOfAlignment2)) == 0);

  // apply the placement
  int numberMapped = applyDepthAndMatchToContig(current, theAssembly, likeNormalizer, libParams);

  // destroy all stored bams
  current=head;
  while(current != NULL) {
    if (current->bamOfAlignment1 != 0)
        bam_destroy1(current->bamOfAlignment1);
    if (current->bamOfAlignment2 != 0)
        bam_destroy1(current->bamOfAlignment2);
    current = current->nextAlignment;
  }
  //fprintf(stderr, "applyPlacement(%s): %d\n", head->name, numberMapped);

  return numberMapped;
}

// an approximation to the digamma function to O(1/x^8)
// http://en.wikipedia.org/wiki/Digamma_function
double digammaApprox(double x){
	double x2 = x*x;
	double x4 = x2*x2;
	double x6 = x4*x2;
    return log(x) - 1.0/(2.0*x) - 1.0/(12.0*x2) + 1.0/(120.0*x4) - 1.0/(252.0*x6);
}

// see http://en.wikipedia.org/wiki/Digamma_function
double negBinom_like(double r, double k_avg, float *k, int N){
    double ans = 0.0;
    int i;
    for(i=0; i<N; i++){
        ans += digammaApprox(k[i] + r);
    }
    ans -= N*digammaApprox(r);
    ans += N*log(r/(r + k_avg));
    return ans;
}

// simple approx
double negBinom_likePrime(double r, double k_avg, float *k, int N, double h){
    return (negBinom_like(r + h, k_avg, k, N) - negBinom_like(r, k_avg, k, N))/h;
}

double negBinom_rFinder(double r_0, double k_avg, float *k, int N, int max_its){
    // through moment matching
    double std_dev = 0.0;
    int i;
    for(i=0; i<N; i++){
    	double kdiff = k_avg - (double)k[i];
        std_dev += kdiff * kdiff;
    }
    std_dev = std_dev/(double)N;
    //printf("rFinder, std=%lf mu=%lf r=%lf\n", std_dev, k_avg, k_avg/(std_dev/k_avg - 1.0));
    return k_avg/(std_dev/k_avg - 1.0);
    
    // through newtons method
    double tol = 1e-6;
    double r = r_0;
    double r_prev;
    int it = 0;
    do {
        it++;
        r_prev = r;
        r = r_prev - negBinom_like(r, k_avg, k, N)/negBinom_likePrime(r, k_avg, k, N, 1e-6);
        if (r < 0){ // kill it
            r = r_0;
            r_prev = r_0;
        }
    } while(it < max_its && fabs(r - r_prev) > tol);
    if(fabs(r - r_prev) > tol){
        //printf("Reached max its without finding params\n");
    }
    //printf("d/dr negBinom_like = %f\n", negBinom_like(r, k_avg, k, N));
    if (isnan(r)) { return r_0; }
    return r;
}

double negBinom_pFinder(double r, double k_avg){
    // by definition
    return k_avg/(r + k_avg);
}

// returns the log of the binomial pmf
// http://en.wikipedia.org/wiki/Gamma-Poisson_distribution#Maximum_likelihood_estimation
double negBinomPMF(int k, double r, double p){
    double ans = 0.0;
    int i;
    for(i = 1; i <= k; i++){
        ans += log(r - 1 + i) - log(i);
    }
    ans += r*log(1.0 - p);
    ans += (double)k*log(p);
    ////printf("pmf k=%d, r=%lf, p=%lf = %lf\n", k, r, p, ans);
    return ans;
}

void applyExpectedMissingLength(assemblyT *theAssembly){
    double avgDepth = theAssembly->depthAvgSum/theAssembly->depthAvgNorm;
    if(avgDepth < minAvgDepth){avgDepth = minAvgDepth;}
    double avgDepthScore = theAssembly->depthScoreAvgSum/theAssembly->depthScoreAvgNorm;
    double avgKmerScore = theAssembly->kmerAvgSum/theAssembly->kmerAvgNorm;
    double avgOverlap = theAssembly->overlapAvgSum/theAssembly->overlapAvgNorm;
    double expectedUnmappedBases = (double)theAssembly->totalUnmappedReads*theAssembly->avgReadSize + (double)theAssembly->totalMappedReads*(theAssembly->avgReadSize - avgOverlap);
    double expectedExtraLength = expectedUnmappedBases/avgDepth;
    printf("Expected extra length: %lf\n", expectedExtraLength);

    // apply avg depth and k-mer score to all positions
    // NORMALIZED NOW, so E[avgDepthScore] = 0, E[avgKmerScore] = 0
    //theAssembly->totalScore += expectedExtraLength*avgDepthScore;
    //theAssembly->totalScore += expectedExtraLength*avgKmerScore;
}

void computeNormaliziedDepthGCParameters(double *depthNormalizer, long *depthNormalizerCount, double *negBinomParam_r, double *negBinomParam_p, double *negBinomParamZnorm_r) {
	int j;
    for(j = 0; j < 101; j++){ // for each GCpct
        if(depthNormalizerCount[j] > 0){
            depthNormalizer[j] = depthNormalizer[j]/(double)depthNormalizerCount[j]; // now contains the avg depth for that GC
        }else{
            depthNormalizer[j] = minAvgDepth;
        }
        if(depthNormalizer[j] < minAvgDepth){
            depthNormalizer[j] = minAvgDepth;
        }
        // through max likelihood/moment matching
        //negBinomParam_r[j] = negBinom_rFinder(depthNormalizer[j], depthNormalizer[j], depthsAtGC, depthNormalizerCount[j], 1000);
        // through constant r
        negBinomParam_r[j] = depthNormalizer[j];
        negBinomParam_p[j] = negBinom_pFinder(negBinomParam_r[j], depthNormalizer[j]);

        if((int)floor(negBinomParam_r[j]) < 2047){
            negBinomParamZnorm_r[j] = negBinomZ[(int)floor(negBinomParam_r[j])];
        }else{
            // not in lookup table, compute
            negBinomParamZnorm_r[j] = getNegBinomZnorm(negBinomParam_r[j]);
        }
    }
}

// compute the depth statistics
int computeDepthStats(assemblyT *theAssembly, libraryParametersT *libParams){
    // 1. Find the GC content of each read
    int i, j, k;
    int GCpct;
    long place;
    double depthNormalizer[102];
    long depthNormalizerCount[102];

    double negBinomParam_p[102];
    double negBinomParam_r[102];
    double negBinomParamZnorm_r[102];
    double tempLogLike;
    long tooLowCoverageBases = 0;
    long noGCInformation = 0;
    unsigned char *GCcont = NULL;

    // initialize
    for(j = 0; j < 101; j++){
        depthNormalizer[j] = 0.0;
        negBinomParam_p[j] = 0.0;
        negBinomParam_r[j] = 0.0;
        negBinomParamZnorm_r[j] = 0.0;
        depthNormalizerCount[j] = 0;
    }
    if (!isMetagenome()) {
        for(i = 0; i < theAssembly->numContigs; i++){ // for each contig
            contig_t *contig = theAssembly->contigs[i];
            if (GCcont != NULL) free(GCcont);
            GCcont = calculateContigGCcont(contig, libParams->avgReadSize);
            for(j = 0; j < contig->seqLen; j++){
                float depth = contig->depth[j];
                if (depth < 0.1) {
                    tooLowCoverageBases++;
                }
                theAssembly->depthAvgSum += depth;
                theAssembly->depthAvgNorm += 1.0;
                GCpct = GCcont[j];
                if (GCpct > 100) {
                    noGCInformation++;
                    continue;
                }
                depthNormalizer[GCpct] += depth;
                depthNormalizerCount[GCpct] += 1;
            }
        }

        // 2. Find the parameters for the distributions

        computeNormaliziedDepthGCParameters(depthNormalizer, depthNormalizerCount, negBinomParam_r, negBinomParam_p, negBinomParamZnorm_r);
    }

    for(i = 0; i < theAssembly->numContigs; i++){ // for each contig
        contig_t *contig = theAssembly->contigs[i];
        if (GCcont != NULL) free(GCcont);
        GCcont = calculateContigGCcont(contig, libParams->avgReadSize);
        if (isMetagenome()) {
        	// re-initialize depth for this contig
        	for(j = 0; j < 101; j++){
        		depthNormalizer[j] = 0.0;
        		negBinomParam_p[j] = 0.0;
        		negBinomParam_r[j] = 0.0;
        		negBinomParamZnorm_r[j] = 0.0;
        		depthNormalizerCount[j] = 0;
        	}

        	for(j = 0; j < contig->seqLen; j++){
        		float depth = contig->depth[j];
        		if (depth < 0.1) {
        			tooLowCoverageBases++;
        		}
        		theAssembly->depthAvgSum += depth;
        		theAssembly->depthAvgNorm += 1.0;
        		GCpct = GCcont[j];
        		if (GCpct > 100) {
        			noGCInformation++;
        			continue;
        		}
        		depthNormalizer[GCpct] += depth;
        		depthNormalizerCount[GCpct] += 1;
        	}

        	// 2. Find the parameters for the distributions

            computeNormaliziedDepthGCParameters(depthNormalizer, depthNormalizerCount, negBinomParam_r, negBinomParam_p, negBinomParamZnorm_r);
        }

        //printf("Calculating likelihoods for %d positions\n", contig->seqLen);
        // 3. Find the depth likelihood
        for(j = 0; j < contig->seqLen; j++){
            GCpct = GCcont[j];
            if (GCpct > 100){
                //printf("location fail %d\n", j);
            	contig->depthLogLikelihood[j] = getMinLogLike();
                continue;
            }

            // compute the depth likelihood using poisson or negBinomial
            // contig->depth[j] is the depth at position j
            // depthNormalizer[GCpct] is avg depth for that GC content

            // tempLike = poissonPMF(contig->depth[j], depthNormalizer[GCpct]); // log poisson pmf
            tempLogLike = negBinomPMF((int)floor(contig->depth[j]), negBinomParam_r[GCpct], 0.5); // log neg binom pmf
            assert(tempLogLike <= 0.0);

            // z normalization
            tempLogLike -= negBinomParamZnorm_r[GCpct];
            //if((int)floor(negBinomParam_r[GCpct]) < 2047){
            //    tempLike -= negBinomZ[(int)floor(negBinomParam_r[GCpct])];
            //}else{
            //    // not in lookup table, compute
            //    tempLike -= getNegBinomZnorm(negBinomParam_r[GCpct]);
            //}
            assert(tempLogLike <= 1.0);


            // thresholding
            if(tempLogLike < getMinLogLike() || isnan(tempLogLike)){
                tempLogLike = getMinLogLike();
            }

            // apply to assembly and totalScore
            contig->depthLogLikelihood[j] = tempLogLike;
            theAssembly->totalScore += tempLogLike; // depth contribution to totalScore at this position
            theAssembly->depthScoreAvgSum += tempLogLike;
            theAssembly->depthScoreAvgNorm += 1.0;

            // at this point contig->matchLikelihood[j] contains the sum of the logs of the TOTAL likelihood of all the reads that map over position j
            // then we take the gemetric average by dividing by the depth and change it (if it is a valid likelihood)
            
            // match
            tempLogLike = contig->matchLogLikelihood[j]/contig->depth[j];
            if(tempLogLike < getMinLogLike() || isnan(tempLogLike) || isinf(tempLogLike)){tempLogLike = getMinLogLike();}
            contig->matchLogLikelihood[j] = tempLogLike;

            // insert
            tempLogLike = contig->insertLogLikelihood[j]/contig->depth[j];
            if(tempLogLike < getMinLogLike() || isnan(tempLogLike)){tempLogLike = getMinLogLike();}
            contig->insertLogLikelihood[j] = tempLogLike;
            
        }
    }
    if (GCcont != NULL) free(GCcont);
    //printf("bases with too low coverage: %ld\n", tooLowCoverageBases);
    //printf("bases with no GC metric (small contigs): %ld\n", noGCInformation);
    return 1;
}

int guessQualityOffset(bam1_t *read) {
  int len = read->core.l_qseq;
  if (len <= 0){
    return -1;
  }
  int qualOffset = -1;
  const int maxExpectedQuality = 50;
  char *qualSeq = (char*) bam1_qual(read);
  int i;
  for(i=0; i < len; i++) {
    if (qualSeq[i] > 64 && qualSeq[i] > 33+maxExpectedQuality){
      qualOffset = 64;
    }
    else if (qualSeq[i] > 0 && qualSeq[i] < 33){
      qualOffset = 0;
    }
    else if (qualSeq[i] > 33 && qualSeq[i] < maxExpectedQuality+33 && qualSeq[i] < 64 && qualSeq[i] > maxExpectedQuality+0){
      qualOffset = 33;
    }

    if (qualOffset >= 0) {
      //printf("guessed quality offset is %d\n", qualOffset);
      break;
    }
  }
  return qualOffset;
}



int compare(int a, int b) {
    if (a==b){
        return 0;
    }else if (a < b){
        return -1;
    }else{
        return 1;
    }
}

int mateTreeCmp(const void *pa, const void *pb) {
  assert(pa != NULL && pb != NULL);
  bam1_t *a = ((bam1_t*) pa);
  bam1_t *b = ((bam1_t*) pb);
  assert((a->core.flag & (BAM_FREAD1 | BAM_FREAD2)) != 0);
  assert((b->core.flag & (BAM_FREAD1 | BAM_FREAD2)) != 0);
  return strcmp(bam1_qname(a), bam1_qname(b));
}


int mateTreeCmpForceStore(const void *pa, const void *pb) {
    int cmp = mateTreeCmp(pa, pb);
    if (cmp == 0) {
        bam1_t *a = ((bam1_t*) pa);
        bam1_t *b = ((bam1_t*) pb);
        cmp = compare(a->core.tid, b->core.tid);
        if (cmp == 0) {
        	cmp = compare(a->core.pos, b->core.pos);
        	if (cmp == 0){
        		//printf("mateTreeCmpForceStore found dup: %s %d %d %d %d %d\n", bam1_qname(a), a->core.tid, a->core.pos, a->core.mtid, a->core.mpos, a->core.isize);
            }
        	return cmp;
        } else
        	return cmp;
    } else
        return cmp;
}

bam1_t *getOrStoreMate(void **mateTree1, void **mateTree2, bam1_t *thisRead) {
    assert(thisRead != NULL);
    void *found = NULL;
    int isRead1 = (thisRead->core.flag & BAM_FREAD1) == BAM_FREAD1;
    int isRead2 = (thisRead->core.flag & BAM_FREAD2) == BAM_FREAD2;
    assert(isRead1 != isRead2 && (isRead1 || isRead2));

    found =  tfind((void*)thisRead, isRead2 ? mateTree1 : mateTree2, mateTreeCmp);

    if (found == NULL) {
        bam1_t *stored = thisRead; //bam_dup1(thisRead);
        if (stored == NULL) {
            printf("ERROR: Unable to store another alignment %d\n", mateTreeCount);
            exit(1);
        }
        void *successful = tsearch((void*) stored, isRead1 ? mateTree1 : mateTree2, mateTreeCmp);
        assert(successful != NULL);
        bam1_t *stored2 = *((bam1_t**) successful);
        assert(stored == stored2);
        mateTreeCount++;
        return NULL;
    } else {
        bam1_t* ret = *((bam1_t**) found);
        void *deleted = tdelete((void*) thisRead, isRead2 ? mateTree1 : mateTree2, mateTreeCmp);
        assert(deleted != NULL);
        mateTreeCount--;
        return ret;
    }
}

void mateTreeApplyRemainderPlacement(const void *nodep, const VISIT which, const int depth) {
    // TODO do not just ignore stragglers, apply their placement as singles (should not get here on proper BAMs)

    bam1_t *bam;
    switch(which) {
    case preorder:
    case endorder:
        break;
    case postorder:
    case leaf:
        bam = *((bam1_t**) nodep);
        //printf("remainder %s %d\n", bam1_qname(bam), bam->core.flag);
        break;
    default:
        //printf("Error %d\n", which);
        exit(1);
    }
}

void mateTreeFreeNode(void *nodep) {
  if (nodep != NULL) {
    mateTreeCount--;
    bam1_t *bam = ((bam1_t*) nodep);
    ////printf("mateTreeFreeNode(%p) freeing (%d) %s\n", nodep, mateTreeCount, bam1_qname(bam));
    bam_destroy1(bam);
  }
}

int isValidInsertSize(bam1_t *thisRead, libraryMateParametersT *mateParameters) {
    // check for > SIGMA_RANGE sigma insert size and fail to chimer
    float range = SIGMA_RANGE * mateParameters->insertStd;
    float mapLen = fabs((float) getFragmentMapLenBAM(thisRead));
    if ((mapLen > mateParameters->insertLength + range)
        || (mapLen < mateParameters->insertLength - range)) {
        return 0;
    } else {
        return 1;
    }
}
void validateAlignmentMates(alignSet_t *thisAlignment, bam1_t *thisRead, bam1_t *thisReadMate) {
    /* print for debugging
    if (thisRead != 0)
      fprintf(stderr, "read1: %s %d: %d %d %d %d\t", bam1_qname(thisRead), thisRead->core.flag, thisRead->core.tid, thisRead->core.mtid, thisRead->core.pos, thisRead->core.mpos);
    if (thisReadMate != 0)
      fprintf(stderr, "read2: %s %d: %d %d %d %d\t", bam1_qname(thisReadMate), thisReadMate->core.flag, thisReadMate->core.tid, thisReadMate->core.mtid, thisReadMate->core.pos, thisReadMate->core.mpos);
    printAlignment(thisAlignment);
    */

    assert(thisAlignment->contigId1 >= 0 && thisAlignment->contigId2 >= 0);
    assert(thisRead != NULL);
    assert(thisReadMate != NULL);

    assert(strcmp(thisAlignment->name, bam1_qname(thisRead)) == 0);
    assert(strcmp(thisAlignment->name, bam1_qname(thisReadMate)) == 0);
    assert(thisRead != thisReadMate);

    assert(thisAlignment->contigId1 == thisRead->core.tid);
    assert(thisAlignment->contigId2 == thisRead->core.mtid);
    assert(thisAlignment->contigId1 == thisReadMate->core.mtid);
    assert(thisAlignment->contigId2 == thisReadMate->core.tid);

    assert(thisAlignment->start1 == thisRead->core.pos);
    assert(thisAlignment->start2 == thisRead->core.mpos);
    assert(thisAlignment->start1 == thisReadMate->core.mpos);
    assert(thisAlignment->start2 == thisReadMate->core.pos);

    // assign the end position, if not set already
    if (thisAlignment->end1 < 0){
        thisAlignment->end1 = bam_calend(&thisRead->core, bam1_cigar(thisRead));
    }
    assert(thisAlignment->end1 == bam_calend(&thisRead->core, bam1_cigar(thisRead)));

    if (thisAlignment->end2 < 0){
        thisAlignment->end2 = bam_calend(&thisReadMate->core, bam1_cigar(thisReadMate));
    }
    assert(thisAlignment->end2 == bam_calend(&thisReadMate->core, bam1_cigar(thisReadMate)));

    assert(thisAlignment->start1 <= thisAlignment->end1);
    assert(thisAlignment->start2 <= thisAlignment->end2);
}

void _setAlignmentForMate(alignSet_t *thisAlignment, bam1_t *read1) {
     if ((read1->core.flag & BAM_FPAIRED) != BAM_FPAIRED || (read1->core.flag & BAM_FMUNMAP) == BAM_FMUNMAP ) {
         // not mapped or not paired
         thisAlignment->start2 = -1;
         thisAlignment->end2 = -1; // no information
         thisAlignment->contigId2 = -1;
     } else {
         thisAlignment->start2 = read1->core.mpos;
         thisAlignment->end2 = -1; // no information
         thisAlignment->contigId2 = read1->core.mtid;
     }
     thisAlignment->bamOfAlignment1 = read1;
     thisAlignment->bamOfAlignment2 = NULL;
}
void _setAlignment(alignSet_t *thisAlignment, bam1_t *read1, bam1_t *read2) {
      assert(read1 != NULL);
      if ((read1->core.flag & BAM_FUNMAP) == BAM_FUNMAP) {
        thisAlignment->start1 = -1;
        thisAlignment->end1   = -1;
        thisAlignment->contigId1 = -1;
      } else {

        thisAlignment->start1 = read1->core.pos;
        thisAlignment->end1   = bam_calend(&read1->core, bam1_cigar(read1));
        assert(thisAlignment->start1 <= thisAlignment->end1);
        thisAlignment->contigId1 = read1->core.tid;
      }

      if (read2 == NULL) {
          _setAlignmentForMate(thisAlignment, read1);
      } else if ((read2->core.flag & BAM_FUNMAP) == BAM_FUNMAP) {
        thisAlignment->start2 = -1;
        thisAlignment->end2   = -1;
        thisAlignment->contigId2 = -1;
      } else {
        thisAlignment->start2 = read2->core.pos;
        thisAlignment->end2   = bam_calend(&read2->core, bam1_cigar(read2));
        assert(thisAlignment->start2 <= thisAlignment->end2);
        thisAlignment->contigId2 = read2->core.tid;
      }

      if (thisAlignment->name != NULL){
          free(thisAlignment->name);
      }
      thisAlignment->name = strdup(bam1_qname(read1));
      thisAlignment->nextAlignment = NULL;
      thisAlignment->likelihood = 1.0;
      thisAlignment->likelihoodInsert = 1.0;
      thisAlignment->bamOfAlignment1 = read1;
      thisAlignment->bamOfAlignment2 = read2;
}

enum MATE_ORIENTATION setAlignment(bam_header_t *header, assemblyT *theAssembly, alignSet_t *thisAlignment, void **mateTree1, void **mateTree2, libraryParametersT *libParams, enum MATE_ORIENTATION orientation, bam1_t *thisRead) {
  assert(thisAlignment != NULL);
  double loglikelihoodRead1 = 0.0;
  double loglikelihoodRead2 = 0.0;
  double logzNormalizeRead1, logzNormalizeRead2;

  double likelihoodInsert = 0.0;
  double likelihoodPlacement = 0.0;

  _setAlignment(thisAlignment, thisRead, NULL);

  // ************* //
  // NORMALIZATION //
  // ************* //

  // divide by the expected likelihood of the read by the normalization factor Z (from Bayes rule)
  // (read-qual normalization happens in setAlignment(q)
  // given only its length and the parameters of the distributions (See paper appendix)

  int qOff = libParams->qOff;
  if (thisAlignment->contigId1 >= 0) {
    //char *md1 = getMD(thisRead, theAssembly->contigs[thisRead->core.tid]->seq);
    //loglikelihoodRead1  = getMatchLogLikelihoodBAM(thisRead, qOff, md1);
    loglikelihoodRead1  = getMatchLogLikelihoodBAM(thisRead, theAssembly->contigs[thisRead->core.tid]->seq, qOff, thisAlignment->end1 - thisAlignment->start1);
    logzNormalizeRead1 = logzNormalizationReadQual(thisRead, qOff);
  }

  //printf("Likelihoods1 (%s): %12f %12f %s\n", bam1_qname(thisRead), loglikelihoodRead1, logzNormalizeRead1, MATE_ORIENTATION_LABELS[orientation]);

  libraryMateParametersT *mateParameters = &libParams->mateParameters[orientation];
  libraryMateParametersT *primaryMateParameters = &libParams->mateParameters[libParams->primaryOrientation];

  switch (orientation) {
    case (VALID_FR):
    case (VALID_RF):
    case (VALID_FF):
    case (NOT_PROPER_FR):
    case (NOT_PROPER_RF):
    case (NOT_PROPER_FF):
      // two reads, both mapped

      assert(thisAlignment->contigId1 >= 0);
      assert(thisAlignment->contigId2 >= 0);

      // valid orientation

      bam1_t *thisReadMate = getOrStoreMate(mateTree1, mateTree2, thisRead);

      if (thisReadMate != NULL) {
          // place this valid pair
          validateAlignmentMates(thisAlignment, thisRead, thisReadMate);

          _setAlignment(thisAlignment, thisRead, thisReadMate);

          //char *md2 = getMD(thisReadMate, theAssembly->contigs[thisReadMate->core.tid]->seq);
          //loglikelihoodRead2 = getMatchLogLikelihoodBAM(thisReadMate, qOff, md2);
          loglikelihoodRead2 = getMatchLogLikelihoodBAM(thisReadMate, theAssembly->contigs[thisReadMate->core.tid]->seq, qOff, thisAlignment->end2 - thisAlignment->start2);
          logzNormalizeRead2 = logzNormalizationReadQual(thisReadMate, qOff);

          //printf("Likelihoods2 (%s): %12f %12f\n", bam1_qname(thisRead), loglikelihoodRead2, logzNormalizeRead2);

          if (mateParameters->isValid == 1) {
              if ( isValidInsertSize(thisRead, mateParameters) ) { // i.e. within SIGMA_RANGE sigma
                  // this is a valid mate pair within a good insert size distribution
                  likelihoodInsert = getInsertLikelihoodBAM(thisRead, mateParameters->insertLength, mateParameters->insertStd) / mateParameters->zNormalizationInsert;
              } else {
                  // cap insert size contribution to SIGMA_RANGE sigma
                  likelihoodInsert = GetCappedInsertProbNormal(SIGMA_RANGE, mateParameters->insertStd) / mateParameters->zNormalizationInsert ; // punish it with SIGMA_RANGE sigma from normal
              }
          } else {
              // not a valid distribution... treat like chimer
              //likelihoodInsert = exp(getMinLogLike()) / primaryMateParameters->zNormalizationInsert; // punish it
              likelihoodInsert = GetCappedInsertProbNormal(SIGMA_RANGE+1.0, primaryMateParameters->insertStd) / primaryMateParameters->zNormalizationInsert; // punish it with SIGMA_RANGE+1 sigma from normal
          }
          likelihoodPlacement = mateParameters->libraryFraction; // orientation likelihood
          likelihoodPlacement *= exp( loglikelihoodRead1 - logzNormalizeRead1 + loglikelihoodRead2 - logzNormalizeRead2 ); // match likelihood
          
          // printf("Likelihood (%s): %12f\n", bam1_qname(thisRead), thisAlignment->likelihood);
          //bam_destroy1(thisReadMate);

          //printf("Applying read mate pair %s. %f %f\n", bam1_qname(thisRead), thisAlignment->likelihood, likelihoodInsert);
      } else {
          // do not process this read yet
          likelihoodPlacement = 0.0;
          likelihoodInsert = 0.0;
          orientation = HALF_VALID_MATE;
      }
      break;

      //  continue... this is actually a chimer
    case (CHIMER) :
      ////printf("WARNING: chimeric read mate pair %s.\n", bam1_qname(thisRead));

      assert(thisAlignment->contigId1 >= 0);
      likelihoodInsert = GetCappedInsertProbNormal(SIGMA_RANGE+1.0, primaryMateParameters->insertStd) / primaryMateParameters->zNormalizationInsert; // punish it with SIGMA_RANGE+1 sigma from normal
      //likelihoodInsert = exp(getMinLogLike()); // punish it for being a chimer

      if (thisRead->core.tid == thisRead->core.mtid && theAssembly->contigs[thisRead->core.tid]->isCircular == 1) {
        // TODO refine based on proximity to end of contigs in a circular genome
        // i.e. factor in: likelihoodThatRead1AndRead2AreCloseToContigEdgeSoAreNotChimersButProbablySpanningMatePairs
      }

      logzNormalizeRead1 = logzNormalizationReadQual(thisRead, libParams->qOff);
      likelihoodPlacement = libParams->totalChimerMateFraction; // orientation likelihood
      likelihoodPlacement *= exp(loglikelihoodRead1 - logzNormalizeRead1);
      break;

    case (READ1_ONLY):
    case (READ2_ONLY):
    case (SINGLE_READ):
      if (thisAlignment->contigId1 < 0) {
          // not aligned
          likelihoodPlacement = 0.0;
          break;
      }

      logzNormalizeRead1 = logzNormalizationReadQual(thisRead, libParams->qOff);
      likelihoodPlacement = libParams->totalValidSingleFraction; // orientation likelihood
      likelihoodPlacement *= exp(loglikelihoodRead1 - logzNormalizeRead1); // match likelihood

      if (orientation == SINGLE_READ) {
          likelihoodInsert = GetInsertProbNormal(0, primaryMateParameters->insertStd) / primaryMateParameters->zNormalizationInsert; // set max normal likelihood
      }else {
          // is a mate pair but other mate did not map.  Treat like chimer
          likelihoodInsert = GetCappedInsertProbNormal(SIGMA_RANGE+1.0, primaryMateParameters->insertStd) / primaryMateParameters->zNormalizationInsert; // punish it with SIGMA_RANGE+1 sigma from normal
          // likelihoodInsert = exp(getMinLogLike());
      }

      //printf("Applying single-ish read %s (%d %s) %f %f\n", bam1_qname(thisRead), orientation, MATE_ORIENTATION_LABELS[orientation], thisAlignment->likelihood, likelihoodInsert);

      break;

    case (UNMAPPED_PAIR):
      // is a mate pair but neither mate did not map.  Treat like chimer
      likelihoodPlacement = 0.0;
      likelihoodInsert = GetCappedInsertProbNormal(SIGMA_RANGE+1.0, primaryMateParameters->insertStd) / primaryMateParameters->zNormalizationInsert; // punish it with SIGMA_RANGE+1 sigma from normal
      // since this is not technically an orientation, no orientation likelihood is applied...
      //likelihoodInsert = exp(getMinLogLike());
      break;

    case (UNRELATED_PAIR):
      //likelihoodInsert = exp(getMinLogLike());
      assert(0); // should not get here!

    default :
      likelihoodPlacement = 0.0;
      likelihoodInsert = 0.0;
      if (thisRead == NULL) { //printf("this read is null!!! Skipping %s\n", MATE_ORIENTATION_LABELS[orientation]);
          break;
      }
      assert(thisRead != NULL);
      //printf("Skipping %s read %s\n", MATE_ORIENTATION_LABELS[orientation], bam1_qname(thisRead));
      break;
  }
  // regardless of normalization, if any of the likelihoods hits the floor it stays at the floor.
  if (loglikelihoodRead1 <= getMinLogLike() || loglikelihoodRead2 <= getMinLogLike() || loglikelihoodRead1+loglikelihoodRead2 <= getMinLogLike() || isnan(likelihoodPlacement) || isinf(likelihoodPlacement)) {
	  likelihoodPlacement = exp(getMinLogLike());
	  //printf("floored placement for %s\n", bam1_qname(thisRead));
  }


  if(orientation == HALF_VALID_MATE) {
	thisAlignment->likelihood = 0.0;
	thisAlignment->likelihoodInsert = 0.0;
  } else {
	thisAlignment->likelihood = likelihoodPlacement;
    thisAlignment->likelihoodInsert = likelihoodInsert;
  }
  assert(thisAlignment->likelihood >= 0.0); // we cannot assume it is less than 1.0 because of the normalization
  assert(thisAlignment->likelihoodInsert >= 0);
  //printf("Likelihoods2 (%s): %12e %s\n", bam1_qname(thisRead), likelihoodInsert, MATE_ORIENTATION_LABELS[orientation]);

  thisAlignment->orientation = orientation; // record this orientation...

  return orientation;
}

// NORMALIZATION
// divide by the expected loglikelihood of the read by the normalization factor Z (from Bayes rule)
// given only its length and the parameters of the distributions (See paper appendix)
double logzNormalizationReadQual(bam1_t *thisRead, int qOff){

  // TODO change for per-base placement
  // return 0.0;

  // find the average quality to save computation/precision in combinatorics
  double Qavg = 0.0;
  char *readQual = (char*) bam1_qual(thisRead);
  int totalLen = thisRead->core.l_qseq;
  int i;
  for(i = 0; i < thisRead->core.l_qseq; i++){
    Qavg += getQtoP(readQual[i], qOff);
  }

  Qavg = Qavg/(double)totalLen;
  double QmisMatch = (1.0 - Qavg)*Qavg; // assume probability goes mostly to next most likely (and that we hit that base)

  double logQavg = log(Qavg);
  double logQmisMatch = log(QmisMatch);

  // normalize over the maximum value to prevent double precision underflows
  double logMaxExpMatch = 2.0*(totalLen-1)*(logQavg > logQmisMatch ? logQavg : logQmisMatch);

  // find the log expected match score
  double tmpExpMatch = 0.0;
  for(i = 0; i < totalLen; i++){
      double logTmp = 2.0*(i*logQavg + (totalLen-i-1)*logQmisMatch);
      tmpExpMatch += exp( logTmp - logMaxExpMatch );
  }
  double logExpMatch = logMaxExpMatch + log(tmpExpMatch);

  ////printf("logQavg: %lf %lf\n", logQavg, logQmisMatch);
  ////printf("logExpMatch: %lf %lf\n",  logExpMatch, logMaxExpMatch);
  ////printf("expMatch: %e %e\n", exp(logExpMatch), exp(logMaxExpMatch));
  //if (exp(logExpMatch) > expMatch * 1.00001 || exp(logExpMatch) < expMatch * 0.999999)
  //      //printf("expMatch: %e %e %e\n", expMatch - exp(logExpMatch), expMatch, exp(logExpMatch));
  return logExpMatch;
}

void countPlacements(int numberMapped, libraryMateParametersT *mateParameters, enum MATE_ORIENTATION orientation, long *placed, long *failedToPlace)
{
    if(numberMapped == 0){
        (*failedToPlace)++;
        mateParameters->unmapped++;
        if(orientation <= IS_PAIRED_ORIENTATION){
            (*failedToPlace)++;
            mateParameters->unmapped++;
        }
    }
    else{
        if(orientation <= IS_PAIRED_ORIENTATION){
            if(numberMapped == 2){
                *placed += 2;
                mateParameters->placed += 2;
            }else{
                (*placed)++;
                mateParameters->placed++;
                (*failedToPlace)++;
                mateParameters->unmapped++;
            }
        }
        else{
            (*placed)++;
            mateParameters->placed++;
        }
    }

}

void computeReadPlacements(samfile_t *ins, assemblyT *theAssembly, libraryParametersT *libParams, samfile_t *placementBam) {
  int i;

  // creates an empty set of containers for reads and alignments
  alignSet_t alignments[N_PLACEMENTS];
  for(i=0; i < N_PLACEMENTS; i++) {
    initAlignment(&alignments[i]);
  }

  int samReadPairIdx = 0;
  int qOff = libParams->qOff;

  alignSet_t *currentAlignment = NULL;
  alignSet_t *head = NULL;

  long failedToPlace = 0;
  long placed = 0;
  long readCount = 0;

  void *mateTree1 = NULL;
  void *mateTree2 = NULL;
  enum MATE_ORIENTATION orientation, lastOrientation;
  int numberMapped;
  int stop = 0;
  while(stop == 0){ // read through all reads
    bam1_t *thisRead = bam_init1(); // allocate new memory for every read
    alignSet_t *thisAlignment = &alignments[samReadPairIdx]; // starts empty
    samReadPairIdx++;

    // reads in the next read from file (ins)
    orientation = readNextBAM(ins, thisRead);

    readCount++;
    if (readCount%1000000 == 0){
      printf("Read %d reads...\n", readCount);
    }
    if (orientation == NO_READS){
      if (head != NULL) {
    	fprintf(stderr, "last alignment.\n");
        numberMapped = applyPlacement(head, theAssembly, libParams);
        countPlacements(numberMapped, &libParams->mateParameters[lastOrientation], lastOrientation, &placed, &failedToPlace);
      }
      break; // end of file
    }

    // populates thisAlignment, finds orientation relative to mate (if any)
    orientation = setAlignment(ins->header, theAssembly, thisAlignment, &mateTree1, &mateTree2, libParams, orientation, thisRead);
    libraryMateParametersT *mateParameters = &libParams->mateParameters[orientation];
    //fprintf(stderr, "Processing %s %d %s\n", bam1_qname(thisRead), orientation, MATE_ORIENTATION_LABELS[orientation]);
    
    if (orientation == UNMAPPED_PAIR) {
      samReadPairIdx--; // overwrite container we just used on next read
      bam_destroy1(thisRead); thisRead = 0;
      //fprintf(stderr, "unmapped pair\n");
      continue;
    }else if (orientation == UNMAPPED_SINGLE) {
      samReadPairIdx--; // overwrite container we just used on next read
      bam_destroy1(thisRead); thisRead = 0;
      //fprintf(stderr, "unmpped single\n");
      continue; // skip
    }else if (orientation == NO_READS){
      bam_destroy1(thisRead); thisRead = 0;
      stop = 1;
      //fprintf(stderr, "no reads\n");
      // need to continue to apply the last alignment
    }else if (orientation == HALF_VALID_MATE) {
      // wait for the mate to be read
      // we placed current half mate in a tree to be retrieved when the mate is found
      // do not destroy thisRead
      samReadPairIdx--; // overwrite container we just used on next read
      //fprintf(stderr, "half valid mate\n");
      continue;
    }else if (thisAlignment->likelihood == 0.0) {
      // do not bother placing, just read the next one.
      //fprintf(stderr, "failed to place: %s\n", thisAlignment->name);
      failedToPlace++;
      mateParameters->unmapped++;
      if (orientation <= CHIMER){
        failedToPlace++;
        mateParameters->unmapped++;
      }
      samReadPairIdx--; // overwrite container we just used on next read
      bam_destroy1(thisRead); thisRead = 0;

      continue;
    }

    // organize linked list of alignments for a specific read based on current state of input stream
    if(currentAlignment == NULL || head == NULL){ // first alignment
      //copyAlignment(&alignments[0], thisAlignment);
      currentAlignment = thisAlignment;
      head = currentAlignment;
      lastOrientation = orientation;
      //fprintf(stderr, "first alignment\n");
    } else if(stop == 0 && thisAlignment != NULL && libParams->isSortedByName == 1 && strcmp(head->name, thisAlignment->name) == 0){
      // if there is more than one placement/mapping/alignment per read the bam file must be sorted by name
      // test to see if this is another alignment of the current set or a new one
      // extend the set of alignments
      currentAlignment->nextAlignment = thisAlignment;
      currentAlignment = thisAlignment;
      //fprintf(stderr, "looking for another alignment\n");
    } else if (head == NULL) {
      assert(0);
    } else {
      numberMapped = applyPlacement(head, theAssembly, libParams);
      //fprintf(stderr, "new alignment: %s\n", thisAlignment->name);
      countPlacements(numberMapped, &libParams->mateParameters[lastOrientation], lastOrientation, &placed, &failedToPlace);

      // set this read as next head
      currentAlignment = &alignments[0];
      copyAlignment(currentAlignment, thisAlignment);
      samReadPairIdx = 1;
      head = currentAlignment;
      lastOrientation = orientation;
    } // end of setting a new alignment

    // make sure we do not overflow the number of placements
    if (samReadPairIdx >= N_PLACEMENTS) {
      printf("WARNING: Exceeded maximum number of duplicate placements: %s\n", thisAlignment->name);
      int previous = N_PLACEMENTS-2;
      currentAlignment = &alignments[previous];
      alignSet_t *tmp = head;
      alignSet_t *leastLikely = head;
      double least = head->likelihood*head->likelihoodInsert;
      while (tmp != NULL) {
        if (tmp->likelihood*tmp->likelihoodInsert < least) {
          least = tmp->likelihood*tmp->likelihoodInsert;
          leastLikely = tmp;
        }
        tmp = tmp->nextAlignment;
      }
      if (thisAlignment->likelihood*thisAlignment->likelihoodInsert > least) {
        // overwrite previous with current
        // printf("WARNING: exceeded maximum placements. Replacing %f with %f\n", leastLikely->likelihood, thisAlignment->likelihood);
        tmp = leastLikely->nextAlignment;
        copyAlignment(leastLikely, thisAlignment);
        leastLikely->nextAlignment = tmp;
      } else {
        // printf("WARNING: exceeded maximum placements.  Dropping low probability placement %f\n", thisAlignment->likelihood);
      }
      currentAlignment->nextAlignment = NULL;
      samReadPairIdx = N_PLACEMENTS-1;
    } // end of placement overflow protection
  } // end of BAM reading

  // tear down array cached variables
  for(i=0; i < N_PLACEMENTS; i++) {
    destroyAlignment(&alignments[i]);
  }

  if (mateTreeCount > 0) {
      printf("There were remaining/missing/orphaned mated reads (%d).\nThese should not exist... Consider fixing your input BAM.\n", mateTreeCount);
      // //printf("Orphaned Read1:\n");
  }
  twalk(mateTree1, mateTreeApplyRemainderPlacement);
  tdestroy(mateTree1,mateTreeFreeNode);

  if (mateTreeCount > 0) {
      //printf("Orphaned Read2 %d\n", mateTreeCount);
  }
  twalk(mateTree2, mateTreeApplyRemainderPlacement);
  tdestroy(mateTree2,mateTreeFreeNode);

  //printf("Destroyed mateTree (%d)\n", mateTreeCount);
  assert(mateTreeCount == 0);

  printf("Summary of placements:\n");
  printf("%i reads placed, %i reads failed to place.\n", placed, failedToPlace);

  for(orientation = 0; orientation < MATE_ORIENTATION_MAX; orientation++) {
    libraryMateParametersT *mateParams = &libParams->mateParameters[orientation];
    if(orientation == READ1_ONLY || orientation == READ2_ONLY){
        mateParams->unmapped = mateParams->unmapped/2; // we double count single mate unmapped
    }
    printf("%s orientation with %ld reads, %ld unmapped, %ld placed, %ld orphaned\n", MATE_ORIENTATION_LABELS[orientation], mateParams->count, mateParams->unmapped, mateParams->placed, mateParams->count - (long)mateParams->unmapped - mateParams->placed);
    theAssembly->totalUnmappedReads += mateParams->unmapped;
    theAssembly->totalMappedReads += mateParams->placed;
  }
  printf("Total unmapped reads: %d\n", theAssembly->totalUnmappedReads);
  theAssembly->totalScore += getMinLogLike()*theAssembly->totalUnmappedReads;
}


#ifndef _GNU_SRC
void tdestroy(void *root, void (*free_node)(void *nodep)) {
  // TODO implement this to fix a memory leak on Mac OS X and others without GNU tdestroy implementations
}
#endif

