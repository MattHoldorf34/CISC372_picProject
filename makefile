image:image.c image.h
	gcc -g image.c -o image -lm

omp:image-openMP.c image.h
	gcc -fopenmp -g image-openMP.c -o image-openMP -lm

pthreads:image-pthread.c image-pthread.h
	gcc -g image-pthread.c -o image-pthread -lm -pthread

darwin: image-pthread.c image-pthread.h image-openMP.c image.h
	gcc -g image-pthread.c -o image-pthread -lm -lpthread -std=c99
	gcc -g image-openMP.c -o image-openMP -lm -fopenmp -std=c99

clean:
	rm -f image output.png image-openMP output-openMP.png image-pthread output-pthread.png
