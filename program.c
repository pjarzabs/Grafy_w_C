#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#define MAX_LINE_LENGTH 65536

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

int calculate_cross_connections(int **matrix, int n, int *groups) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (matrix[i][j] && groups[i] != groups[j]) {
                count++;
            }
        }
    }
    return count;
}

void divide_graph_into_groups(int **matrix, int n, FILE *fout) {
    if (n < 3) {
        fprintf(fout, "Graf musi miec co najmniej 3 wierzcholki do podzialu.\n");
        return;
    }

    int *groups = malloc(n * sizeof(int));
    int *best_groups = malloc(n * sizeof(int));
    if (!groups || !best_groups) {
        fprintf(stderr, "Blad alokacji pamieci dla grup.\n");
        if (groups) free(groups);
        if (best_groups) free(best_groups);
        return;
    }

    // Initialize with a simple division
    int group_size = n / 3;
    for (int i = 0; i < n; i++) {
        groups[i] = i / group_size;
        if (groups[i] > 2) groups[i] = 2;
    }

    memcpy(best_groups, groups, n * sizeof(int));
    int min_cross_connections = calculate_cross_connections(matrix, n, groups);

    int improved;
    do {
        improved = 0;
        
        for (int i = 0; i < n; i++) {
            int original_group = groups[i];
            int best_group = original_group;
            int best_score = min_cross_connections;
            
            for (int g = 0; g < 3; g++) {
                if (g == original_group) continue;
                
                groups[i] = g;
                int current_score = calculate_cross_connections(matrix, n, groups);
                
                if (current_score < best_score) {
                    best_score = current_score;
                    best_group = g;
                }
            }
            
            groups[i] = best_group;
            
            if (best_score < min_cross_connections) {
                min_cross_connections = best_score;
                memcpy(best_groups, groups, n * sizeof(int));
                improved = 1;
            } else {
                groups[i] = original_group;
            }
        }
    } while (improved);

    int group_counts[3] = {0};
    for (int i = 0; i < n; i++) {
        group_counts[best_groups[i]]++;
    }

    fprintf(fout, "\nPodzial grafu na 3 grupy:\n");
    fprintf(fout, "Liczba polaczen miedzy grupami: %d\n", min_cross_connections);
    fprintf(fout, "Liczba wierzcholkow w grupach: %d, %d, %d\n", 
            group_counts[0], group_counts[1], group_counts[2]);
    
    for (int g = 0; g < 3; g++) {
        fprintf(fout, "Grupa %d: ", g);
        int first = 1;
        for (int i = 0; i < n; i++) {
            if (best_groups[i] == g) {
                if (!first) fprintf(fout, ", ");
                fprintf(fout, "%d", i);
                first = 0;
            }
        }
        fprintf(fout, "\n");
    }

    free(groups);
    free(best_groups);
}

int** read_adjacency_matrix(FILE *fin, int *n) {
    char line[MAX_LINE_LENGTH];
    int **matrix = NULL;
    *n = 0;

    // Pomijamy nagłówek "Macierz sąsiedztwa:"
    while (fgets(line, MAX_LINE_LENGTH, fin)) {
        if (strstr(line, "Macierz s")) {
            break;
        }
    }

    // Odczytujemy wiersze macierzy
    int row = 0;
    while (fgets(line, MAX_LINE_LENGTH, fin)) {
        trim(line);
        if (line[0] != '[') break; // Koniec macierzy

        // Pierwszy wiersz - alokacja pamięci
        if (*n == 0) {
            // Liczymy liczbę kolumn (liczbę liczb w wierszu)
            char *temp = strdup_local(line);
            char *token = strtok(temp, " []."); // Separatory: spacja, [, ], .
            int count = 0;
            while (token != NULL) {
                count++;
                token = strtok(NULL, " [].");
            }
            free(temp);
            *n = count;

            // Alokacja macierzy n x n
            matrix = malloc(*n * sizeof(int*));
            for (int i = 0; i < *n; i++) {
                matrix[i] = calloc(*n, sizeof(int));
            }
        }

        // Parsowanie wiersza
        char *temp = strdup_local(line);
        char *token = strtok(temp, " [].");
        int col = 0;
        while (token != NULL && col < *n) {
            matrix[row][col] = (atoi(token) != 0) ? 1 : 0; // Konwersja na 0/1
            col++;
            token = strtok(NULL, " [].");
        }
        free(temp);
        row++;
    }

    return matrix;
}
int main(int argc, char *argv[]) {
    
    printf("\n  Po wpisaniu %s w linii polecen, podaj nazwe pliku, \n  ktory zawiera polaczenia dzialow firmy w formie grafu,", argv[0]);
    printf("\n  a nastepnie nazwe pliku wynikowego. Przyklad polecenia:\n  %s oddzialy_firmy.csrrg optymalne.txt\n", argv[0]);

    if (argc == 4 && strcmp(argv[1], "generate") == 0) {
        int n = atoi(argv[2]);
        double p = atof(argv[3]);
        generate_random_graph(n, p, "nowy_graf.txt");
        return 0;
    }

    if(argc != 3) {
        fprintf(stderr, "\n\n\nBlad! Poprawny sposob uzycia: %s oddzialy_firmy.csrrg optymalne.txt\n", argv[0]);
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

    int n;
    int **matrix = read_adjacency_matrix(fin, &n);
    fclose(fin);

    if (matrix == NULL) {
        fprintf(stderr, "Blad odczytu macierzy sasiedztwa z pliku.\n");
        fclose(fout);
        return 1;
    }

    fprintf(fout, "Graf:\n");
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

    divide_graph_into_groups(matrix, n, fout);

    for (int i = 0; i < n; i++) {
        free(matrix[i]);
    }
    free(matrix);
    fclose(fout);

    printf("\n\n  Zoptymalizowano polaczenia komunikacyjne poszczegolnych dzialow firmy i zapisano w pliku %s.\n ", argv[2]);
    printf(" Teraz zarzadzanie Wasza firma bedzie przebiegalo sprawniej. \n  Nie zapomnijcie przypisac osoby nadzorujacej do nowo utworzonych dzialow!\n");
    
    return 0;
}
