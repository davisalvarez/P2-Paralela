#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <math.h>
#include <rpc/des_crypt.h>

void decrypt(long key, char *ciph, int len){
	long k = 0;

	for(int i=0; i<8; ++i){
		key <<= 1;
		k += (key & (0xFE << i*8));
	}

	des_setparity((char *)&k);
	ecb_crypt((char *)&k, (char *) ciph, 16, DES_DECRYPT);
}

void encrypt(long key, char *ciph, int len){
	long k = 0;

	for(int i=0; i<8; ++i){
		key <<= 1;
		k += (key & (0xFE << i*8));
	}

	des_setparity((char *)&k);
	ecb_crypt((char *)&k, (char *) ciph, 16, DES_ENCRYPT);
}


char search[] = " Diego ";


int tryKey(long key, char *ciph, int len){
	char temp[len+1];
	memcpy(temp, ciph, len);
	temp[len]=0;

	decrypt(key, temp, len);

	return strstr((char *)temp, search) != NULL;
}


int main(int argc, char *argv[]){
	double start, finish;
	int N, id;
	long upper = (1L << 56); //upper bound DES keys 2^56
  long key =pow(2, 56)/2+1;
	long mylower, myupper;

	MPI_Status st;
	MPI_Request req;

	FILE *file;
	char *buffer = malloc(sizeof(char) * 1024);
	char *cipher = malloc(sizeof(char) * 1024);

	file = fopen("mensaje.txt", "r");
	if (file == NULL) return -1;

	int i = 0;
	while((buffer[i] = fgetc(file)) != EOF){
		cipher[i] = buffer[i];
		i++;

		if (i + 1 > 1024) break;
	}

	cipher[i] = '\0';

	free(buffer);

	int ciphlen = strlen(cipher);

	encrypt(key, cipher, ciphlen);

	MPI_Comm comm = MPI_COMM_WORLD;

	MPI_Init(NULL, NULL);
	start = MPI_Wtime();

	MPI_Comm_size(comm, &N);
	MPI_Comm_rank(comm, &id);

	long range_per_node = upper / N;

	mylower = range_per_node * id;
	myupper = range_per_node * (id + 1) - 1;

	if(id == N - 1){
    //compensar residuo
		myupper = upper;
	}

	long found = 0;
	int ready = 0;

	MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  if (id != 0){
    for(long i = mylower; i<myupper; ++i){
      MPI_Test(&req, &ready, MPI_STATUS_IGNORE);

      if(ready) 
        break;  //ya encontraron, salir

      if(tryKey(i, (char *)cipher, ciphlen)){
        found = i;
        for(int node = 0; node < N; node++){
          MPI_Send(&found, 1, MPI_LONG, node, 0, MPI_COMM_WORLD);
        }
        break;
      }
    }
  }

	else{
		MPI_Wait(&req, &st);
		decrypt(found, (char *)cipher, ciphlen);
		finish = MPI_Wtime();
		printf("Found is: %li\n", found);
		printf("Work took %f s\n", finish - start);
	}

	MPI_Finalize();
}