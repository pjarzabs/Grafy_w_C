#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024

void trim(char *str) {
    size_t len = strlen(str);
    while(len > 0 && (str[len-1]=='\n' || str[len-1]=='\r' || isspace(str[len-1]))) {
        str[len-1] = '\0';
        len--;
    }
}

char *strdup_local(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

int* parse_int_array(const char *line, int *count) {
    int capacity = 10;
    int *array = malloc(capacity * sizeof(int));
    *count = 0;
    char *temp = strdup_local(line);
    char *token = strtok(temp, " ;\t\r\n");
    while(token != NULL) {
        if(*count >= capacity) {
            capacity *= 2;
            array = realloc(array, capacity * sizeof(int));
        }
        array[*count] = atoi(token);
        (*count)++;
        token = strtok(NULL, " ;\t\r\n");
    }
    free(temp);
    return array;
}

void generate_random_graph(int n, double p, const char *output_filename) {
    if (n <= 0) {
        fprintf(stderr, "Liczba wierzcholkow musi byc dodatnia.\n");
        return;
    }
    if (p < 0.0 || p > 1.0) {
        fprintf(stderr, "Prawdopodobienstwo musi miescic sie w przedziale [0.0, 1.0].\n");
        return;
    }

    FILE *fout = fopen(output_filename, "w");
    if (!fout) {
        fprintf(stderr, "Blad: Nie mozna otworzyc pliku do zapisu: %s\n", output_filename);
        return;
    }

    int **matrix = malloc(n * sizeof(int*));
    if (!matrix) {
        fprintf(stderr, "Blad alokacji pamieci dla wierszy macierzy.\n");
        fclose(fout);
        return;
    }

    for (int i = 0; i < n; i++) {
        matrix[i] = calloc(n, sizeof(int));
        if (!matrix[i]) {
            fprintf(stderr, "Blad alokacji pamieci dla kolumn macierzy (wiersz %d).\n", i);
            for (int j = 0; j < i; j++) free(matrix[j]);
            free(matrix);
            fclose(fout);
            return;
        }
    }

    srand((unsigned)time(NULL));

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if ((double)rand() / RAND_MAX < p) {
                matrix[i][j] = 1;
                matrix[j][i] = 1;
            }
        }
    }

    fprintf(fout, "Macierz s\xC4\x85siedztwa:\n");
    for (int i = 0; i < n; i++) {
        fprintf(fout, "[");
        for (int j = 0; j < n; j++) {
            fprintf(fout, "%d.", matrix[i][j]);
            if (j < n - 1) fprintf(fout, " ");
        }
        fprintf(fout, "]\n");
    }

    fprintf(fout, "\nLista polaczen:\n");
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (matrix[i][j]) {
                fprintf(fout, "%d - %d\n", i, j);
            }
        }
    }

    for (int i = 0; i < n; i++) {
        free(matrix[i]);
    }
    free(matrix);
    fclose(fout);

    printf("Graf zostal pomyslnie wygenerowany (%d wierzcholkow, p=%.2f) do pliku: %s\n", n, p, output_filename);
}

int main(int argc, char *argv[]) {
    if (argc == 4 && strcmp(argv[1], "generate") == 0) {
        int n = atoi(argv[2]);
        double p = atof(argv[3]);
        generate_random_graph(n, p, "NowyGraf.txt");
        return 0;
    }

    if(argc != 3) {
        fprintf(stderr, "Sposob uzycia: %s input.csrrg output.txt\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    if(!fin) {
        fprintf(stderr, "Blad otwarcia pliku wejsciowego: %s\n", argv[1]);
        return 1;
    }

    FILE *fout = fopen(argv[2], "w");
    if(!fout) {
        fprintf(stderr, "Blad otwarcia pliku wyjsciowego: %s\n", argv[2]);
        fclose(fin);
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    int graphCount = 0;

    while(1) {
        char line1[MAX_LINE_LENGTH], line2[MAX_LINE_LENGTH], line3[MAX_LINE_LENGTH];
        char line4[MAX_LINE_LENGTH], line5[MAX_LINE_LENGTH];

        if(fgets(line1, MAX_LINE_LENGTH, fin) == NULL) break;
        if(fgets(line2, MAX_LINE_LENGTH, fin) == NULL) break;
        if(fgets(line3, MAX_LINE_LENGTH, fin) == NULL) break;
        if(fgets(line4, MAX_LINE_LENGTH, fin) == NULL) break;
        if(fgets(line5, MAX_LINE_LENGTH, fin) == NULL) break;

        trim(line1); trim(line2); trim(line3); trim(line4); trim(line5);

        int max_nodes = atoi(line1);

        int countIndices;
        int *indices = parse_int_array(line2, &countIndices);

        int countRowPtr;
        int *rowPtr = parse_int_array(line3, &countRowPtr);
        int n = countRowPtr - 1;

        int **matrix = malloc(n * sizeof(int*));
        for(int i = 0; i < n; i++) {
            matrix[i] = calloc(n, sizeof(int));
        }

        for (int i = 0; i < n; i++) {
            for (int j = rowPtr[i]; j < rowPtr[i+1]; j++) {
                int col = indices[j];
                if(col < 0 || col >= n) {
                    fprintf(stderr, "Ostrzezenie: indeks kolumny %d poza zakresem dla wiersza %d (n=%d). Pomijam.\n", col, i, n);
                    continue;
                }
                matrix[i][col] = 1;
            }
        }

        fprintf(fout, "Graf %d:\n", ++graphCount);
        fprintf(fout, "Macierz s\xC4\x85siedztwa:\n");
        for (int i = 0; i < n; i++) {
            fprintf(fout, "[");
            for (int j = 0; j < n; j++) {
                fprintf(fout, "%d.", matrix[i][j]);
                if(j < n - 1) fprintf(fout, " ");
            }
            fprintf(fout, "]\n");
        }
        fprintf(fout, "\nLista polaczen:\n");
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if(matrix[i][j] == 1) {
                    fprintf(fout, "%d - %d\n", i, j);
                }
            }
        }
        fprintf(fout, "\n--------------------------------\n\n");

        free(indices);
        free(rowPtr);
        for (int i = 0; i < n; i++) {
            free(matrix[i]);
        }
        free(matrix);
    }

    fclose(fin);
    fclose(fout);

    printf("Konwersja zakonczona. Przetworzono %d graf(ow).\n", graphCount);
    return 0;
}
