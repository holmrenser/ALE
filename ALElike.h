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

// finds the insert probability (assuming normal distribution) P(point | N(0,sigma))
double GetInsertProbNormal(const double sigma, const double point){
  //printf("Point: %f, p1: %f, p2: %f\n", point, erf((point + 0.5)/sqrt(2*sigma*sigma)), erf((point - 0.5)/sqrt(2*sigma*sigma)));
  return 0.5*(erf((abs(point) + 0.5)/sqrt(2*sigma*sigma)) - erf((abs(point) - 0.5)/sqrt(2*sigma*sigma)));
}

double getInsertLikelihood(SAM_t *read, float mu, float var){
    return GetInsertProbNormal(abs((float)read->mapLen) - mu, var);
}

// finds the likelihood of a string of misses in read from seqPos to matchLen
double likeMiss(SAM_t *read, int seqPos, int missLen){
    int i;
    double likelihood = 1.0;
    for(i = seqPos; i < seqPos + missLen; i++){
        likelihood = likelihood*((1.0 - QtoP[read->readQual[i] - 64])/3.0);
    }
    return likelihood;
}

// finds the likelihood of a string of matches in read from seqPos to matchLen
double likeMatch(SAM_t *read, int seqPos, int matchLen){
    int i;
    double likelihood = 1.0;
    for(i = seqPos; i < seqPos + matchLen; i++){
        likelihood = likelihood*(QtoP[read->readQual[i] - 64]);
    }
    return likelihood;
}

// takes in a read and returns the match likelihood (due to matches, mismatches, indels)
double getMatchLikelihood(SAM_t *read){
    
    int stop = 0;
    int pos = 5;
    int seqPos = 0;
    int base = 1;
    int matchLen, missLen, delLen, insLen;
    int totalMatch = 0, totalMiss = 0, totalDel = 0, totalIns = 0;
    int hardClipped = 0;
    double likelihood = 1.0;
    // parse MD field
    while(stop == 0){
        // matches
        matchLen = 0;
        while(isdigit(read->MD[pos])){
            matchLen = matchLen*10 + hackedIntCast(read->MD[pos]);
            pos++;
        }
        totalMatch += matchLen;
        likelihood = likelihood*likeMatch(read, seqPos, matchLen);
        seqPos += matchLen;
        // misses
        missLen = 0;
        while(read->MD[pos] == 'A' || read->MD[pos] == 'T' || read->MD[pos] == 'C' || read->MD[pos] == 'G' || read->MD[pos] == '0'){
            if(read->MD[pos] == 'A' || read->MD[pos] == 'T' || read->MD[pos] == 'C' || read->MD[pos] == 'G'){
                missLen++;
                likelihood = likelihood*likeMiss(read, seqPos, 1);
                seqPos++;
            }
            if(read->MD[pos] == 'N'){
                missLen++;
                likelihood = likelihood*0.25;
                seqPos++;
            }
            pos++;
        }
        totalMiss += missLen;
        // deletions
        delLen = 0;
        if(read->MD[pos] == '^'){
            pos++;
            while(isalpha(read->MD[pos])){
                delLen++;
                pos++;
            }
        }
        totalDel += delLen;
        // sees if we are at the end
        if(read->MD[pos] == '\0'){
            stop = 1;
        }
    }
    // parse CIGAR
    stop = 0;
    pos = 0;
    int num;
    while(stop == 0){
        // matches
        num = 0;
        while(isdigit(read->cigar[pos])){
            num = num*10 + hackedIntCast(read->cigar[pos]);
            pos++;
        }
        // insertions
        if(read->cigar[pos] == 'I'){
            totalIns += num;
        }
        pos++;
        // read to the end
        if(read->cigar[pos] == '\0'){
            stop = 1;
        }
    }
    if(totalDel + totalIns > 0){
        likelihood = 0.0; // can modify later to include insertions and deletions
    }else if(totalMatch == 0){
        likelihood = 0.0;
    }
    printf("Found %i match(es).\n", totalMatch);
    printf("Found %i miss(es).\n", totalMiss);
    printf("Found %i deletion(s).\n", totalDel);
    printf("Found %i insertion(s).\n", totalIns);
    printf("Likelihood: %f.\n", likelihood);
    return likelihood;
}

int kmerHash(char c1, int place){
    int add1 = pow(2, 2*place);
    int add2 = add1*2;
    if(c1 == 'A'){
        return 0;
    }else if(c1 == 'T'){
        return add1;
    }else if(c1 == 'C'){
        return add2;
    }else if(c1 == 'G'){
        return add1 + add2;
    }else{
        return -100000;
    }
}

int getKmerHash(char *seq, int startPos, int kmerLen){
    int i;
    int hash = 0;
    for(i = 0; i < kmerLen; i++){
        hash += kmerHash(seq[startPos+i], i);
    }
    return hash;
}

int computeKmerStats(assemblyT *theAssembly, int kmer){
    int i, j, k, totalKmers, hash;
    // calculate total possible kmers
    totalKmers = 1;
    for(i = 0; i < kmer; i++){
        totalKmers = totalKmers*4;
    }
    int *kmerVec = malloc(totalKmers*sizeof(int));
    // find all kmers present
    totalKmers = 0;
    for(i = 0; i < theAssembly->numContigs; i++){
        // initialize kmerVec
        for(j = 0; j < totalKmers; j++){
            kmerVec[j] = 0;
        }
        // add up the kmers
        for(j = 0; j < theAssembly->contigs[i].seqLen - kmer + 1; j++){
            hash = getKmerHash(theAssembly->contigs[i].seq, j, kmer);
            //printf("Hash = %i\n", hash);
            if(hash > -1){
                kmerVec[hash]++;
                totalKmers++;
            }
        }
        //printf("Calculated all %i kmers!\n", totalKmers);
        // calculate probability of seeing that kmer based on the rest of the contig
        // first kmer - 1 unrolled
        for(j = 0; j < kmer; j++){
            theAssembly->contigs[i].kmerLikelihood[j] = 0.0;
            for(k = 0; k < j+1; k++){
                hash = getKmerHash(theAssembly->contigs[i].seq, k, kmer);
                if(hash > -1){
                    theAssembly->contigs[i].kmerLikelihood[j] = theAssembly->contigs[i].kmerLikelihood[j] + 1.0/(float)(j+1)*(float)(kmerVec[hash])/(float)(totalKmers);
                }
            }
            //printf("New likelihood[%i]: %f.\n", j, theAssembly->contigs[i].kmerLikelihood[j]);
        }
        //printf("First.\n");
        // middle bunch
        for(j = kmer; j < theAssembly->contigs[i].seqLen - kmer; j++){
            for(k = 0; k < kmer; k++){
                hash = getKmerHash(theAssembly->contigs[i].seq, j - k, kmer);
                if(hash > -1){
                    theAssembly->contigs[i].kmerLikelihood[j] = theAssembly->contigs[i].kmerLikelihood[j] + 1.0/(float)(kmer)*(float)(kmerVec[hash])/(float)(totalKmers);
                }
            }
            //printf("New likelihood[%i]: %f.\n", j, theAssembly->contigs[i].kmerLikelihood[j]);
        }
        //printf("Mid.\n");
        // last bits
        for(j = theAssembly->contigs[i].seqLen - kmer; j < theAssembly->contigs[i].seqLen; j++){
            theAssembly->contigs[i].kmerLikelihood[j] = 0.0;
            for(k = j - kmer + 1; k < j - kmer + 1 + (theAssembly->contigs[i].seqLen - j); k++){
                hash = getKmerHash(theAssembly->contigs[i].seq, k, kmer);
                if(hash > -1){
                    theAssembly->contigs[i].kmerLikelihood[j] = theAssembly->contigs[i].kmerLikelihood[j] + 1.0/(float)(theAssembly->contigs[i].seqLen - j)*(float)(kmerVec[hash])/(float)(totalKmers);
                }
            }
            //printf("New likelihood[%i]: %f.\n", j, theAssembly->contigs[i].kmerLikelihood[j]);
        }
        //printf("Last.\n");
        totalKmers = 0;
    }
}