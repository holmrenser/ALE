#define mapLens_MAX 20000
#define GCmaps_MAX 400

struct contig_struct{
    char name[256];
    int seqLen;
    char *seq;
    double *depth;
    double *matchLikelihood;
    double *kmerLikelihood;
    double *depthLikelihood;
    double *GCcont;
};

struct assembly_struct{
    int numContigs;
    int totalAssemLen;
    struct contig_struct **contigs;
};

struct SAMalignment{ // values in order of read in
    char readName[256];
    int outInfo;
    char refName[256];
    int mapStart;
    int mapPair;
    char cigar[256];
    char flag2[10];
    int mapEnd;
    int mapLen;
    char readSeq[256];
    char readQual[256];
    char XA[256];
    char MD[256];
    char NM[256];
    struct SAMalignment *pair;
};

struct setOfAlignments{
    double likelihood;
    int start1, start2;
    int end1, end2;
    char name[256];
    char mapName[256];
    struct setOfAlignments *nextAlignment;
};


typedef struct setOfAlignments alignSet_t;
typedef struct SAMalignment SAM_t;
typedef struct contig_struct contig_t;
typedef struct assembly_struct assemblyT;

struct _contig_ll {
	contig_t *contig;
	void *next;
};
typedef struct _contig_ll contig_ll;

void copyAlignment(alignSet_t *dst, const alignSet_t *src) {
	assert(dst != NULL);
	assert(src != NULL);
	dst->likelihood = src->likelihood;
	dst->start1 = src->start1;
	dst->start2 = src->start2;
	dst->end1 = src->end1;
	dst->end2 = src->end2;
	strcpy(dst->name, src->name);
	strcpy(dst->mapName, src->mapName);
	dst->nextAlignment = src->nextAlignment;
}

void swap(void **x, void **y) {
	void *t = *x;
	*x = *y;
	*y = t;
}

void printAssembly(assemblyT *theAssembly);

// int sizeOfContig(int len){
//     return 256*sizeof(char) + sizeof(int) + len*(sizeof(char) + 5*sizeof(double));
// }

int getGCtotal(char seq1[], int seq1len){
    int GCtot = 0, i;
    for(i = 0; i < seq1len; i++){
        if(toupper(seq1[i]) == 'G' || toupper(seq1[i]) == 'C'){
            GCtot++;
        }
    }
    return GCtot;
}

int findNumAssemPieces(kseq_t *ins){
    int l, count = 0;
    while ((l = kseq_read(ins)) >= 0) {
        count++;
    }
    return count;
}

void readAssembly(kseq_t *ins, assemblyT *theAssembly){
    int contigLen, i, l, j = 0;
    contig_ll *tmp, *head = malloc(sizeof(contig_ll));
    head->contig = NULL;
    head->next = NULL;
    tmp = head;

    while ((l = kseq_read(ins)) >= 0) {
        contigLen = (int)(ins->seq.l);
        printf("Found contig %d, contigLen = %i\n", j, contigLen);

        contig_t *contig = tmp->contig = (contig_t*) malloc(sizeof(contig_t));
        contig->seqLen = contigLen;
        contig->seq = malloc(contigLen*sizeof(char));
        contig->depth = malloc(contigLen*sizeof(double));
        contig->matchLikelihood = malloc(contigLen*sizeof(double));
        contig->kmerLikelihood = malloc(contigLen*sizeof(double));
        contig->depthLikelihood = malloc(contigLen*sizeof(double));
        contig->GCcont = malloc(contigLen*sizeof(double));
        strcpy(contig->name, ins->name.s);
        for(i = 0; i < contigLen; i++){
        	contig->seq[i] = toupper(ins->seq.s[i]);
        	contig->depth[i] = 0.0;
        	contig->matchLikelihood[i] = 0.0;
        	contig->kmerLikelihood[i] = 0.0;
        	contig->depthLikelihood[i] = 0.0;
        	contig->GCcont[i] = 0.0;
        }
        j++;
        tmp->next = malloc(sizeof(contig_ll));
        tmp = tmp->next;
        tmp->contig = NULL;
        tmp->next = NULL;

    }
    int numberAssemblyPieces = j;

    theAssembly->contigs = (contig_t**)malloc((numberAssemblyPieces)*sizeof(contig_t*));
    theAssembly->numContigs = numberAssemblyPieces;
    tmp = head;
    i = 0;
    while (tmp != NULL) {
    	if (tmp->contig != NULL)
    		theAssembly->contigs[i++] = tmp->contig;
    	head = tmp;
    	tmp = head->next;
    	free(head);
    }
}

// below is my attempt at a hanning window convolution, I coded it from scratch so watch for bugs!
void calculateGCcont(assemblyT *theAssembly, int avgReadSize){
	int i, j, baseGC;
	int *GCpast = malloc(sizeof(int)*avgReadSize);
	for(i = 0; i < theAssembly->numContigs; i++){
		contig_t *contig = theAssembly->contigs[i];
		baseGC = getGCtotal(contig->seq, avgReadSize);
		GCpast[0] = baseGC;
		for(j = 0; j < avgReadSize; j++){
			contig->GCcont[j] = (double)baseGC/(double)((j+1)*avgReadSize);
			GCpast[(j+1)%avgReadSize] = GCpast[j%avgReadSize];
			if(contig->seq[j] == 'G' || contig->seq[j] == 'C'){
				GCpast[(j+1)%avgReadSize]--;
			}
			if(contig->seq[j+avgReadSize] == 'G' || contig->seq[j+avgReadSize] == 'C'){
				GCpast[(j+1)%avgReadSize]++;
			}
			baseGC += GCpast[(j+1)%avgReadSize];
		}
		for(j = avgReadSize; j < contig->seqLen - avgReadSize; j++){
			contig->GCcont[j] = (double)baseGC/(double)(avgReadSize*avgReadSize);
			baseGC -= GCpast[(j+1)%avgReadSize];
			GCpast[(j+1)%avgReadSize] = GCpast[j%avgReadSize];
			if(contig->seq[j] == 'G' || contig->seq[j] == 'C'){
				GCpast[(j+1)%avgReadSize]--;
			}
			if(contig->seq[j+avgReadSize] == 'G' || contig->seq[j+avgReadSize] == 'C'){
				GCpast[(j+1)%avgReadSize]++;
			}
			baseGC += GCpast[(j+1)%avgReadSize];
		}
		for(j = contig->seqLen - avgReadSize; j < contig->seqLen; j++){
			contig->GCcont[j] = (double)baseGC/(double)((contig->seqLen - j)*avgReadSize);
			baseGC -= GCpast[(j+1)%avgReadSize];
		}
	}
}

void printAssembly(assemblyT *theAssembly){
    int i;
    for(i = 0; i < theAssembly->numContigs; i++){
    	contig_t *contig = theAssembly->contigs[i];
        printf("Contig %i: %s: %s\n",i, contig->name, contig->seq);
    }
}

// prints out the SAM alignment
void printSAM(SAM_t read){
    printf("SAM alignment:\n");
    printf("%s %i %s %i %i %s %s %i %i %s %s %s %s %s\n", read.readName, read.outInfo, read.refName, read.mapStart, read.mapPair, read.cigar, read.flag2, read.mapEnd, read.mapLen, read.readSeq, read.readQual, read.XA, read.MD, read.NM);
}

// loads a SAM formatted text line into a SAM_t record
int loadSamLine(char *samLine, SAM_t *read) {
	int offset = 0;
	int keepGoing = sscanf( samLine, "%255s%i%255s%i%i%255s%10s%i%i%255s%255s%255s%n", read->readName, &read->outInfo, read->refName, &read->mapStart, &read->mapPair, read->cigar, read->flag2, &read->mapEnd, &read->mapLen, read->readSeq, read->readQual, read->XA, &offset);
	if(keepGoing < 1)
		return 0;

	if(read->cigar[0] != '*'){ // see if there is an actual alignment there
	    keepGoing = sscanf( samLine+offset, "%255s%255s", read->MD, read->NM);
	}else{
	    strcpy(read->MD, "MD:Z:0");
	    strcpy(read->NM, "NM:i:0");
	}
	return keepGoing;
}

// returns the length of a sequence, does not take indels into account
int getSeqLen(char seq[256]){
    int i;
    for(i = 0; i < 256; i++){
        if(seq[i] != 'A' && seq[i] != 'T' && seq[i] != 'C' && seq[i] != 'G'){
            //printf("Len: %i\n", i);
            return i;
        }
    }
    printf("wtf?");
    return 0;
}

int readMatesBAM(samfile_t *ins, bam1_t *read1, bam1_t *read2) {
	assert(ins != NULL);
	assert(read1 != NULL);
	assert(read2 != NULL);

	int bytesRead = samread(ins, read1);
	//printf("1: %s %d\n", bam1_qname(read1), bytesRead);
	if (bytesRead <= 0)
		return 0;
    if ( (read1->core.flag & BAM_FPAIRED) == BAM_FPAIRED ) {
    	bytesRead = samread(ins, read2);
    	//printf("2: %s %d\n", bam1_qname(read2), bytesRead);
    	if (bytesRead <= 0) {
    		printf("WARNING: missing mate to %s\n", bam1_qname(read1));
    		return 1;
    	}
    	if (strcmp( bam1_qname(read1), bam1_qname(read2) ) != 0) {
    		printf("WARNING: Read out-of-order mate pairs: %s %s\n", bam1_qname(read1), bam1_qname(read2));
    	}
        //printf("read 2 mated reads %s %s\n", bam1_qname(read1), bam1_qname(read2));
    	return 2;
    } else {
    	//printf("WARNING: %s is not a paired read\n", bam1_qname(read1));
    	return 1; // not a paired read
    }
    printf("WARNING: How did you get here?\n");
    return 0;
}

int getSeqLenBAM(bam1_t *read) {
	assert(read != NULL);
	return bam_cigar2qlen(&read->core, bam1_cigar(read));
}

int getMapLenBAM(bam1_t *read1, bam1_t *read2) {
	assert(read1 != NULL);
	assert(read2 != NULL);

	int left = read1->core.pos < read2->core.pos ? read1->core.pos : read2->core.pos;
	int right1 = bam_calend(&read1->core, bam1_cigar(read1));
	int right2 = bam_calend(&read2->core, bam1_cigar(read2));
	int right = right1 > right2 ? right1 : right2;
	assert(right >= left);
	return right - left;
}

char *getTargetName(samfile_t *ins, bam1_t *read) {
	assert(ins != NULL);
	assert(read != NULL);
	return ins->header->target_name[ read->core.tid ];
}
// prints out all of the alignments in the linked list
void printAlignments(alignSet_t *head){
    // print out the head
    printf("Alignment 1 for read %s: %f at %i-%i and %i-%i.\n", head->name, head->likelihood, head->start1, head->end1, head->start2, head->end2);
    alignSet_t *current = head;
    int i = 1;
    while(current->nextAlignment != NULL){
        current = current->nextAlignment;
        i++;
        printf("Alignment %i for read %s: %f at %i-%i and %i-%i.\n", i, current->name, current->likelihood, current->start1, current->end1, current->start2, current->end2);
    }
}

void writeToOutput(assemblyT *theAssembly, FILE *out){
    int i, j;
    for(i = 0; i < theAssembly->numContigs; i++){
    	contig_t *contig = theAssembly->contigs[i];
        fprintf(out, ">%s %i > depth ln(depthLike) ln(placeLike) ln(kmerLike) ln(totalLike)\n", contig->name, contig->seqLen);
        for(j = 0; j < contig->seqLen; j++){
            fprintf(out, "%lf %lf %lf %lf %lf\n", contig->depth[j], contig->depthLikelihood[j], log(contig->matchLikelihood[j]), log(contig->kmerLikelihood[j]), contig->depthLikelihood[j] + log(contig->matchLikelihood[j]) + log(contig->kmerLikelihood[j]));
        }
    }
}

int assemblySanityCheck(assemblyT *theAssembly){
    int i, j, num = theAssembly->numContigs;
    int error = 1;
    for(j=0; j < num ; j++){
    	contig_t *contig = theAssembly->contigs[j];
        for(i = 0; i < contig->seqLen; i++){
            if(contig->seq[i] != 'A' && contig->seq[i] != 'T' && contig->seq[i] != 'C' && contig->seq[i] != 'G'){
                printf("Found an error in the assembly, contig %d, position %d = %c\n", j, i, contig->seq[i]);
                error = 0;
            }
        }
    }
    if(error == 0){
      printf("ALE considers these errors and will treat them as such; leaving a low depth, kmer score and placement likelihood around these regions. ALE only accepts the bases A,T,C,G,N.\n");
    }
    return error;
}

assemblyT *loadAssembly(char *filename) {

    // attempt to open the input file
    gzFile *assemblyFile = gzopen(filename, "r");
    kseq_t *Aseq;
    if(assemblyFile == NULL){
        printf("Error! Could not open assembly file: %s\n", filename);
        exit(1);
    }

    assemblyT *theAssembly = malloc(sizeof(assemblyT));
    if (theAssembly == NULL)
    	exit(1);

    Aseq = kseq_init(assemblyFile);

    readAssembly(Aseq, theAssembly);
    printf("Done reading in assembly.\n");

    //printAssembly(theAssembly);
    assemblySanityCheck(theAssembly);
    gzclose(assemblyFile);

    return theAssembly;
}

void freeContig(contig_t *contig) {
	if (contig == NULL)
		return;
	free(contig->seq);
	free(contig->depth);
	free(contig->matchLikelihood);
	free(contig->kmerLikelihood);
	free(contig->depthLikelihood);
	free(contig->GCcont);
	free(contig);
}

void freeAssembly(assemblyT *theAssembly) {
	if (theAssembly != NULL) {
		if (theAssembly->contigs != NULL) {
			int i;
			for (i = 0; i < theAssembly->numContigs; i++)
				freeContig(theAssembly->contigs[i]);
			free(theAssembly->contigs);
		}
		free(theAssembly);
	}
}
