/*
 ============================================================================
 Name        : izp-gaussian.c
 Author      : Andraz Vrhovec
 Version     :
 Copyright   : Use it, abuse it!
 Description : Gaussian filter
 ============================================================================
 */

#ifdef DEBUG
// fukn debug direktive tuki
#define USE_SSE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pam.h>
#include <libgen.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#ifdef USE_SSE
#include <pmmintrin.h>
#endif
#include <valgrind/callgrind.h>

#define EXTENSION_BORDER 8
#define FILTER_SIZE 7

#define PROFILE_RDTSC 1

typedef struct {
	float a;
	float b;
	float c;
	float d;
} SSEfloat;

float** izp_extendImage(unsigned int **image, const int cols, const int rows,
		int *newCols, int *newRows);

void** izp_allocarray(const int width, const int height, const int size);

#ifdef USE_SSE
void izp_convolve1Dsse(float **image, float *vec, const int cols,
		const int rows, const int size);
void izp_convolve2Dsse(float **image, float **mat, const int cols,
		const int rows, const int matSize);
#endif

void izp_convolve1D(float **image, float *vec, const int cols, const int rows,
		const int size);

void izp_convolve2D(float **image, float **mat, const int cols, const int rows,
		const int matSize);

float ** izp_gaussianMatrix(const int n, const int m, const float sigma);

float** izp_transpose(float **image, const int cols, const int rows);

unsigned int** izp_toUintArray(float **image, const int cols, const int rows);

static inline unsigned long long rdtsc(void);

int cols;
int rows;
int newCols;
int newRows;
gray maxval;
gray **image = NULL;

int main(int argc, char **argv) {

	pgm_init(&argc, argv);

	if (argc < 2) {
		char *prog = basename(argv[0]);
		printf("Usage:\n"
				"\t ./%s <PGM image>\n", prog);
		return EXIT_SUCCESS;
	}

	// open image file
	printf("Opening image: %s\n", argv[1]);
	FILE *fp = fopen(argv[1], "r");

	if (fp == NULL) {
		fprintf(stderr, "Error reading file %s\n", (char*) argv[1]);
		return EXIT_FAILURE;
	}

	// read image
	image = pgm_readpgm(fp, &cols, &rows, &maxval);
	printf("Image read! Width: %d Height: %d\n", rows, cols);

	// make extended image (SSE alignment, border for convolution)
	float** extendedImage = izp_extendImage(image, cols, rows, &newCols,
			&newRows);

	// free old buffer
	free(image[0]);
	free(image);

#ifdef USE_2D
	float **gauss = izp_gaussianMatrix(FILTER_SIZE, FILTER_SIZE, 3.0f);
#else
	float **gaussRow = izp_gaussianMatrix(1, FILTER_SIZE, 3.0f);
#endif

#ifdef PROFILE_RDTSC
	unsigned long long start = rdtsc();
#endif

#ifdef USE_SSE
#ifdef USE_2D
	printf("Using 2D SSE\n");
	izp_convolve2Dsse(extendedImage, gauss, cols, rows, FILTER_SIZE);
#else
	printf("Using 1D SSE\n");

	// convolve horizontal
	izp_convolve1Dsse(extendedImage, gaussRow[0], cols, rows, FILTER_SIZE);

	// transpose
	float** transposed = izp_transpose(extendedImage, newCols, newRows);
	// free old buffers
	free(extendedImage[0]);
	free(extendedImage);

	// convolve verticl (use same kernel as image is transposed)
	izp_convolve1Dsse(transposed, gaussRow[0], rows, cols, FILTER_SIZE);

	// transpose back
	extendedImage = izp_transpose(transposed, newRows, newCols);
#endif
#else

#ifdef USE_2D
	printf("Using 2D non-SSE\n");
	izp_convolve2D(extendedImage, gauss, cols, rows, FILTER_SIZE);
#else
	printf("Using 1D non-SSE\n");

	float row[7] = {0.10629f,   0.14032f,   0.16577f,   0.17524f,   0.16577f,   0.14032f,   0.10629f};

	izp_convolve1D(extendedImage, row, cols, rows, FILTER_SIZE);

	float** transposed = izp_transpose(extendedImage, newCols, newRows);
	// free old buffers
	free(extendedImage[0]);
	free(extendedImage);

	izp_convolve1D(transposed, gaussRow[0], rows, cols, FILTER_SIZE);
	extendedImage = izp_transpose(transposed, newRows,
			newCols);
#endif
#endif

#ifdef PROFILE_RDTSC
	unsigned long long cycles = rdtsc() - start;
	printf("%lld cycles/pixel \t %f seconds/image.\n", cycles / (rows * cols),
			(float) cycles / (2 * 1E9));
#endif

	// convert image back to unsigned int
	unsigned int** uintImage = izp_toUintArray(extendedImage, newCols, newRows);

	// write it
	FILE *fpOut = fopen("output.pgm", "w");
	if (fpOut != NULL) {
		pgm_writepgm(fpOut, (gray**) uintImage, newCols, newRows, maxval, 0);
	}

	free(extendedImage[0]);
	free(extendedImage);

	free(uintImage[0]);
	free(uintImage);

	printf("Great success!\n");
	return EXIT_SUCCESS;
}

inline void izp_convolve2D(float** restrict image, float** restrict mat,
		const int cols, const int rows, const int matSize) {

	float tmp = 0.0f;

	for (int i = EXTENSION_BORDER; i < rows + EXTENSION_BORDER; ++i) {
		for (int j = EXTENSION_BORDER; j < cols + EXTENSION_BORDER; ++j) {
			tmp = 0.0f;
			// 1st row
			tmp += image[i - 3][j - 3] * mat[0][0];
			tmp += image[i - 3][j - 2] * mat[0][1];
			tmp += image[i - 3][j - 1] * mat[0][2];
			tmp += image[i - 3][j] * mat[0][3];
			tmp += image[i - 3][j + 1] * mat[0][4];
			tmp += image[i - 3][j + 2] * mat[0][5];
			tmp += image[i - 3][j + 3] * mat[0][6];

			// 2nd row
			tmp += image[i - 2][j - 3] * mat[1][0];
			tmp += image[i - 2][j - 2] * mat[1][1];
			tmp += image[i - 2][j - 1] * mat[1][2];
			tmp += image[i - 2][j] * mat[1][3];
			tmp += image[i - 2][j + 1] * mat[1][4];
			tmp += image[i - 2][j + 2] * mat[1][5];
			tmp += image[i - 2][j + 3] * mat[1][6];

			// 3rd row
			tmp += image[i - 1][j - 3] * mat[2][0];
			tmp += image[i - 1][j - 2] * mat[2][1];
			tmp += image[i - 1][j - 1] * mat[2][2];
			tmp += image[i - 1][j] * mat[2][3];
			tmp += image[i - 1][j + 1] * mat[2][4];
			tmp += image[i - 1][j + 2] * mat[2][5];
			tmp += image[i - 1][j + 3] * mat[2][6];

			// 4th row
			tmp += image[i][j - 3] * mat[3][0];
			tmp += image[i][j - 2] * mat[3][1];
			tmp += image[i][j - 1] * mat[3][2];
			tmp += image[i][j] * mat[3][3];
			tmp += image[i][j + 1] * mat[3][4];
			tmp += image[i][j + 2] * mat[3][5];
			tmp += image[i][j + 3] * mat[3][6];

			// 5th row
			tmp += image[i + 1][j - 3] * mat[4][0];
			tmp += image[i + 1][j - 2] * mat[4][1];
			tmp += image[i + 1][j - 1] * mat[4][2];
			tmp += image[i + 1][j] * mat[4][3];
			tmp += image[i + 1][j + 1] * mat[4][4];
			tmp += image[i + 1][j + 2] * mat[4][5];
			tmp += image[i + 1][j + 3] * mat[4][6];

			// 6th row
			tmp += image[i + 2][j - 3] * mat[5][0];
			tmp += image[i + 2][j - 2] * mat[5][1];
			tmp += image[i + 2][j - 1] * mat[5][2];
			tmp += image[i + 2][j] * mat[5][3];
			tmp += image[i + 2][j + 1] * mat[5][4];
			tmp += image[i + 2][j + 2] * mat[5][5];
			tmp += image[i + 2][j + 3] * mat[5][6];

			// 5th row
			tmp += image[i + 3][j - 3] * mat[6][0];
			tmp += image[i + 3][j - 2] * mat[6][1];
			tmp += image[i + 3][j - 1] * mat[6][2];
			tmp += image[i + 3][j] * mat[6][3];
			tmp += image[i + 3][j + 1] * mat[6][4];
			tmp += image[i + 3][j + 2] * mat[6][5];
			tmp += image[i + 3][j + 3] * mat[6][6];

			image[i][j] = tmp;
		}
	}
}

void izp_convolve1D(float** restrict image, float* restrict vec, const int cols,
		const int rows, const int size) {

	register float tmp = 0.0f;

	for (int i = EXTENSION_BORDER; i < rows + EXTENSION_BORDER; ++i) {
		for (int j = EXTENSION_BORDER; j < cols + EXTENSION_BORDER; ++j) {
			tmp = 0.0f;

			tmp += image[i][j - 3] * vec[0];
			tmp += image[i][j - 2] * vec[1];
			tmp += image[i][j - 1] * vec[2];
			tmp += image[i][j] * vec[3];
			tmp += image[i][j + 1] * vec[4];
			tmp += image[i][j + 2] * vec[5];
			tmp += image[i][j + 3] * vec[6];

			image[i][j] = tmp;
		}

	}
}

#ifdef USE_SSE

inline void izp_convolve2Dsse(float** restrict image, float** restrict mat,
		const int cols, const int rows, const int matSize) {

//	how not to implement SSE :D

	float m[8][8];
	memcpy(&m[0][0], &mat[0][0], sizeof(float) * 7);
	memcpy(&m[1][0], &mat[1][0], sizeof(float) * 7);
	memcpy(&m[2][0], &mat[2][0], sizeof(float) * 7);
	memcpy(&m[3][0], &mat[3][0], sizeof(float) * 7);
	memcpy(&m[4][0], &mat[4][0], sizeof(float) * 7);
	memcpy(&m[5][0], &mat[5][0], sizeof(float) * 7);
	memcpy(&m[6][0], &mat[6][0], sizeof(float) * 7);

	__m128 m00 = _mm_load_ps(&m[0][0]);
	__m128 m01 = _mm_load_ps(&m[0][4]);

	__m128 m10 = _mm_load_ps(&m[1][0]);
	__m128 m11 = _mm_load_ps(&m[1][4]);

	__m128 m20 = _mm_load_ps(&m[2][0]);
	__m128 m21 = _mm_load_ps(&m[2][4]);

	__m128 m30 = _mm_load_ps(&m[3][0]);
	__m128 m31 = _mm_load_ps(&m[3][4]);

	__m128 m40 = _mm_load_ps(&m[4][0]);
	__m128 m41 = _mm_load_ps(&m[4][4]);

	__m128 m50 = _mm_load_ps(&m[5][0]);
	__m128 m51 = _mm_load_ps(&m[5][4]);

	__m128 m60 = _mm_load_ps(&m[6][0]);
	__m128 m61 = _mm_load_ps(&m[6][4]);

	float x;
	float tmp[4];

	__m128 zoki;

	for (int i = EXTENSION_BORDER; i < rows + EXTENSION_BORDER; ++i) {
		for (int j = EXTENSION_BORDER; j < cols + EXTENSION_BORDER; j = j+1) {

			x = 0.0f;

			// 1st row
			__m128 a = _mm_loadu_ps(&image[i - 3][j - 4]);
			__m128 b = _mm_loadu_ps(&image[i - 3][j]);

			zoki = _mm_add_ps(_mm_mul_ps(b, m01), _mm_mul_ps(a, m00));

			// 2nd row
			__m128 c = _mm_loadu_ps(&image[i - 2][j - 4]);
			__m128 d = _mm_loadu_ps(&image[i - 2][j]);

			zoki = _mm_add_ps(zoki, _mm_mul_ps(c, m10));
			zoki = _mm_add_ps(zoki, _mm_mul_ps(d, m11));


			// 3rd row
			__m128 e = _mm_loadu_ps(&image[i - 1][j - 4]);
			__m128 f = _mm_loadu_ps(&image[i - 1][j]);

			_mm_add_ps(zoki, _mm_mul_ps(e, m20));
			_mm_add_ps(zoki, _mm_mul_ps(f, m21));
			// 4th row

			__m128 g = _mm_loadu_ps(&image[i][j - 4]);
			__m128 h = _mm_loadu_ps(&image[i][j]);

			zoki = _mm_add_ps(zoki, _mm_mul_ps(g, m30));
			zoki = _mm_add_ps(zoki, _mm_mul_ps(h, m31));

			// 5th row
			__m128 ii = _mm_loadu_ps(&image[i + 1][j - 4]);
			__m128 jj = _mm_loadu_ps(&image[i + 1][j]);

			zoki = _mm_add_ps(zoki, _mm_mul_ps(ii, m40));
			zoki = _mm_add_ps(zoki, _mm_mul_ps(jj, m41));

			// 6th row
			__m128 k = _mm_loadu_ps(&image[i + 2][j - 4]);
			__m128 l = _mm_loadu_ps(&image[i + 2][j]);

			zoki = _mm_add_ps(zoki, _mm_mul_ps(k, m50));
			zoki = _mm_add_ps(zoki, _mm_mul_ps(l, m51));

			// 7th row
			__m128 mm = _mm_loadu_ps(&image[i + 3][j - 4]);
			__m128 nn = _mm_loadu_ps(&image[i + 3][j]);

			zoki = _mm_add_ps(zoki, _mm_mul_ps(mm, m60));
			zoki = _mm_add_ps(zoki, _mm_mul_ps(nn, m61));

			_mm_store_ss((float*)&image[i][j], _mm_hadd_ps(zoki, zoki));


		}
	}
}

void izp_convolve1Dsse(float** restrict image, float* restrict vec,
		const int cols, const int rows, const int size) {

	float coef[15];

	coef[0] = 0.0f;
	coef[1] = 0.0f;
	coef[2] = 0.0f;
	coef[3] = 0.0f;
	coef[4] = vec[0];
	coef[5] = vec[1];
	coef[6] = vec[2];
	coef[7] = vec[3];
	coef[8] = vec[4];
	coef[9] = vec[5];
	coef[10] = vec[6];
	coef[11] = 0.0f;
	coef[12] = 0.0f;
	coef[13] = 0.0f;
	coef[14] = 0.0f;

	const __m128 vec0_lo = _mm_loadu_ps(&coef[3]);
	const __m128 vec0_mi = _mm_loadu_ps(&coef[7]);

	const __m128 vec1_lo = _mm_loadu_ps(&coef[2]);
	const __m128 vec1_mi = _mm_loadu_ps(&coef[6]);
	const __m128 vec1_hi = _mm_loadu_ps(&coef[10]);

	const __m128 vec2_lo = _mm_loadu_ps(&coef[1]);
	const __m128 vec2_mi = _mm_loadu_ps(&coef[5]);
	const __m128 vec2_hi = _mm_loadu_ps(&coef[9]);

	const __m128 vec3_mi = _mm_loadu_ps(&coef[4]);
	const __m128 vec3_hi = _mm_loadu_ps(&coef[8]);

	__m128 tmp0;
	__m128 tmp1;
	__m128 tmp2;
	__m128 tmp3;

	__m128 a;
	__m128 b;
	__m128 c;

	__m128 ZOKI;
	__m128 BOKI;

	for (int i = EXTENSION_BORDER; i < rows + EXTENSION_BORDER; ++i) {
		a = _mm_load_ps(&image[i][EXTENSION_BORDER - 4]);
		for (int j = EXTENSION_BORDER; j < cols + EXTENSION_BORDER; j = j + 4) {

			// 1st pass with vec0
			b = _mm_load_ps(&image[i][j]);
			c = _mm_load_ps(&image[i][j + 4]);

			tmp0 = _mm_add_ps(_mm_mul_ps(a, vec0_lo), _mm_mul_ps(b, vec0_mi));
			tmp1 = _mm_add_ps(
					_mm_add_ps(_mm_mul_ps(a, vec1_lo), _mm_mul_ps(b, vec1_mi)),
					_mm_mul_ps(c, vec1_hi));
			tmp3 = _mm_add_ps(_mm_mul_ps(b, vec3_mi), _mm_mul_ps(c, vec3_hi));
			tmp2 = _mm_add_ps(
					_mm_add_ps(_mm_mul_ps(a, vec2_lo), _mm_mul_ps(b, vec2_mi)),
					_mm_mul_ps(c, vec2_hi));

			ZOKI = _mm_hadd_ps(_mm_hadd_ps(tmp0, tmp0),
					_mm_hadd_ps(tmp1, tmp1));
			BOKI = _mm_hadd_ps(_mm_hadd_ps(tmp2, tmp2),
					_mm_hadd_ps(tmp3, tmp3));

			a = _mm_shuffle_ps(ZOKI, BOKI, 0xCC);
			_mm_store_ps(&image[i][j], a);

		}
	}
}
#endif

float** izp_extendImage(unsigned int** restrict image, const int cols,
		const int rows, int *newCols, int *newRows) {

	*newCols = cols + EXTENSION_BORDER + EXTENSION_BORDER;
	*newRows = rows + EXTENSION_BORDER + EXTENSION_BORDER;

	if ((*newCols % 4) != 0) {
		printf("Image width is not multiple of 4, padding!\n");
		*newCols = ((*newCols + 4) & ~0x3);
	}

	if ((*newRows % 4) != 0) {
		printf("Image height is not multiple of 4, padding!\n");
		*newRows = ((*newRows + 4) & ~0x3);
	}

	// allocate big cosy array :D
	float **extImage = (float**) izp_allocarray(*newCols, *newRows,
			sizeof(float));

	// copy centred
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			extImage[i + EXTENSION_BORDER][j + EXTENSION_BORDER] =
					(float) image[i][j];
		}
	}

	return extImage;
}

float** izp_transpose(float** restrict image, const int cols, const int rows) {

	// allocate big cosy array :D
	float **extImage = (float **) izp_allocarray(rows, cols, sizeof(float));

// copy centred
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			extImage[j][i] = image[i][j];
		}
	}

	return extImage;
}

unsigned int** izp_toUintArray(float** restrict image, const int cols,
		const int rows) {

	// allocate big cosy array :D
	unsigned int **extImage = (unsigned int **) izp_allocarray(cols, rows,
			sizeof(unsigned int));

	// copy centred
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			extImage[i][j] = (int) image[i][j];
		}
	}

	return extImage;
}

void** izp_allocarray(const int width, const int height, const int size) {

	void** idx = (void **) memalign(32, (height + 1) * sizeof(void*));
	void* heap = memalign(32, width * height * size);

	if (idx != NULL && heap != NULL) {
		for (int i = 0; i < height; ++i) {
			idx[i] = &(heap[i * width * size]);
		}
		idx[height] = heap;
		return idx;
	} else {
		fprintf(stderr, "alloc2DArray failed!");
		return NULL;
	}
}

float ** izp_gaussianMatrix(const int n, const int m, const float sigma) {

	float ** g = (float **) izp_allocarray(m, n, sizeof(float));
	float sum = 0.0f;
	const int yoff = (n - 1) / 2;
	const int xoff = (m - 1) / 2;

	float tmp = 0.0f;
	for (int y = 0; y < n; ++y) {
		for (int x = 0; x < m; ++x) {
			tmp = exp(
					-((y - yoff) * (y - yoff) + (x - xoff) * (x - xoff))
							/ (2 * sigma * sigma));
			g[y][x] = tmp;
			sum += tmp;
		}
	}

	// normalize
	for (int y = 0; y < n; ++y) {
		for (int x = 0; x < m; ++x) {
			g[y][x] /= sum;
		}
	}

	return g;
}

void izp_printMatrix(float** mat, const int n, const int m) {
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < m; j++) {
			printf("%6f ", mat[i][j]);
		}
		printf("\n");
	}
}

static inline unsigned long long rdtsc(void) {
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long) lo) | (((unsigned long long) hi) << 32);
}

