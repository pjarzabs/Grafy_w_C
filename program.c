#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <math.h>


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


int** read_adjacency_matrix(FILE *fin, int *n) {
    char line[MAX_LINE_LENGTH];
    
    // Read number of vertices
    if (!fgets(line, MAX_LINE_LENGTH, fin)) return NULL;
    *n = atoi(line);
    if (*n <= 0) return NULL;

    // Read edges
    if (!fgets(line, MAX_LINE_LENGTH, fin)) return NULL;
    int edge_count;
    int *edges = parse_int_array(line, &edge_count);

    // Create empty matrix
    int **matrix = malloc(*n * sizeof(int*));
    if (!matrix) return NULL;
    
    for(int i = 0; i < *n; i++) {
        matrix[i] = calloc(*n, sizeof(int));
        if (!matrix[i]) {
            for(int j = 0; j < i; j++) free(matrix[j]);
            free(matrix);
            return NULL;
        }
    }

    // Populate matrix (assuming edges are pairs)
    for(int i = 0; i < edge_count; i += 2) {
        if(i+1 >= edge_count) break;
        int v1 = edges[i];
        int v2 = edges[i+1];
        if(v1 >= 0 && v1 < *n && v2 >= 0 && v2 < *n) {
            matrix[v1][v2] = 1;
            matrix[v2][v1] = 1; // Undirected graph
        }
    }

    free(edges);
    return matrix;
}

int calculate_cross_connections(int **matrix, int n, int *groups) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (matrix[i][j] && groups[i] != groups[j]) {
                count++;
            }
        }
    }
    return count / 2; // Each connection counted twice
}

void divide_into_balanced_groups(int **matrix, int n, FILE *fout) {
    int *groups = malloc(n * sizeof(int));
    int *best_groups = malloc(n * sizeof(int));
    if (!groups || !best_groups) {
        fprintf(stderr, "Błąd alokacji pamięci\n");
        return;
    }

    // Inicjalizacja z lepszym balansem
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        groups[i] = i % 3;  // Początkowo równomierny podział
    }

    int min_cross = calculate_cross_connections(matrix, n, groups);
    memcpy(best_groups, groups, n * sizeof(int));

    int max_iter = 100; 
    float balance_weight = 0.1;  // Waga dla kryterium balansu

    for (int iter = 0; iter < max_iter; iter++) {
        int improved = 0;
        
        for (int v = 0; v < n; v++) {
            int current_group = groups[v];
            int best_group = current_group;
            int current_score = min_cross;
            
  
            int sizes[3] = {0};
            for (int i = 0; i < n; i++) sizes[groups[i]]++;
            
            for (int g = 0; g < 3; g++) {
                if (g == current_group) continue;
                
                groups[v] = g;
                int new_cross = calculate_cross_connections(matrix, n, groups);
                
                int new_sizes[3] = {0};
                for (int i = 0; i < n; i++) new_sizes[groups[i]]++;
                
                int balance_cost = abs(new_sizes[0] - n/3) + 
                                 abs(new_sizes[1] - n/3) + 
                                 abs(new_sizes[2] - n/3);
                
                int new_score = new_cross + balance_weight * balance_cost;
                
                if (new_score < current_score) {
                    current_score = new_score;
                    best_group = g;
                }
            }
            
            if (best_group != current_group) {
                groups[v] = best_group;
                min_cross = calculate_cross_connections(matrix, n, groups);
                improved = 1;
                
                if (min_cross < calculate_cross_connections(matrix, n, best_groups)) {
                    memcpy(best_groups, groups, n * sizeof(int));
                }
            } else {
                groups[v] = current_group;
            }
        }
        
        if (!improved) break;
    }

    int counts[3] = {0};
    for (int i = 0; i < n; i++) counts[best_groups[i]]++;

    if (counts[0] == 0 || counts[1] == 0 || counts[2] == 0) {
        for (int g = 0; g < 3; g++) {
            if (counts[g] == 0) {
                int max_g = (counts[0] > counts[1]) ? 0 : 1;
                max_g = (counts[max_g] > counts[2]) ? max_g : 2;
                
                for (int i = 0; i < n; i++) {
                    if (best_groups[i] == max_g) {
                        best_groups[i] = g;
                        counts[max_g]--;
                        counts[g]++;
                        break;
                    }
                }
            }
        }
    }

    min_cross = calculate_cross_connections(matrix, n, best_groups);

    fprintf(fout, "\nOptymalny podział na 3 grupy:\n");
    fprintf(fout, "Minimalna liczba połączeń między grupami: %d\n", min_cross);

    counts[0] = counts[1] = counts[2] = 0;
    for (int i = 0; i < n; i++) counts[best_groups[i]]++;
    fprintf(fout, "Liczebność grup: %d, %d, %d\n", counts[0], counts[1], counts[2]);

    for (int g = 0; g < 3; g++) {
        fprintf(fout, "Grupa %d: ", g);
        for (int i = 0; i < n; i++) {
            if (best_groups[i] == g) fprintf(fout, "%d ", i);
        }
        fprintf(fout, "\n");
    }

    free(groups);
    free(best_groups);
}
void generate_random_graph(int n, int edges, const char *output_filename) {
    srand(time(NULL));
    
    FILE *file = fopen(output_filename, "w");
    if (file == NULL) {
        printf("Nie można otworzyć pliku do zapisu.\n");
        return;
    }
    
    int **adj_matrix = (int **)malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        adj_matrix[i] = (int *)calloc(n, sizeof(int));
    }
    
    int edges_generated = 0;
    
    // Najpierw zapewniamy, że każdy wierzchołek ma przynajmniej jedno połączenie
    for (int i = 0; i < n; i++) {
        int connected = 0;
        
        for (int j = 0; j < n; j++) {
            if (adj_matrix[i][j] || adj_matrix[j][i]) {
                connected = 1;
                break;
            }
        }
        
        if (!connected) {
            int attempts = 0;
            while (attempts < n * 2) {
                int to = rand() % n;
                if (i != to && adj_matrix[i][to] == 0) {
                    adj_matrix[i][to] = 1;
                    edges_generated++;
                    break;
                }
                attempts++;
            }
        }
    }
    
    // Generujemy pozostałe krawędzie
    while (edges_generated < edges) {
        int from = rand() % n;
        int to = rand() % n;
        
        if (from != to && adj_matrix[from][to] == 0) {
            adj_matrix[from][to] = 1;
            edges_generated++;
        }
    }
    
    // Zapis macierzy sąsiedztwa do pliku (bez kropek)
    fprintf(file, "Graf:\n");
    fprintf(file, "Macierz sąsiedztwa:\n");
    for (int i = 0; i < n; i++) {
        fprintf(file, "[");
        for (int j = 0; j < n; j++) {
            fprintf(file, "%d", adj_matrix[i][j]); // Usunięta kropka
            if (j < n - 1) {
                fprintf(file, " ");
            }
        }
        fprintf(file, "]\n");
    }
    fprintf(file, "\n");
    
    // Zapis listy połączeń do pliku
    fprintf(file, "Lista polaczen:\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (adj_matrix[i][j] == 1) {
                fprintf(file, "%d - %d\n", i, j);
            }
        }
    }
    
    fclose(file);
    for (int i = 0; i < n; i++) {
        free(adj_matrix[i]);
    }
    free(adj_matrix);
}

int main(int argc, char *argv[]) {
    // Wyświetl pomoc jeśli brak argumentów
    if (argc == 1) {
        printf("\nPoprawne sposoby uzycia:\n");
        printf("1. Generowanie grafu:\n");
        printf("   %s generate liczba_wierzcholkow liczba_krawedzi plik_wyjsciowy.txt\n", argv[0]);
        printf("2. Optymalizacja grafu:\n");
        printf("   %s plik_wejsciowy.csrrg plik_wyjsciowy.txt\n\n", argv[0]);

        return 0;
    }

    // Tryb generowania grafu
    if (argc == 5 && strcmp(argv[1], "generate") == 0) {
        int n = atoi(argv[2]);
        int edges = atoi(argv[3]);
        const char *filename = argv[4];
        generate_random_graph(n, edges, filename);
        return 0;
    }
    // Tryb optymalizacji
    else if (argc == 3) {
        const char *input_file = argv[1];
        const char *output_file = argv[2];
        
        FILE *fin = fopen(input_file, "r");
        if (!fin) {
            fprintf(stderr, "Blad otwarcia pliku wejsciowego: %s\n", input_file);
            return 1;
        }

        FILE *fout = fopen(output_file, "w");
        if (!fout) {
            fprintf(stderr, "Blad otwarcia pliku wyjsciowego: %s\n", output_file);
            fclose(fin);
            return 1;
        }

        int n;
        int **matrix = read_adjacency_matrix(fin, &n);
        fprintf(fout, "Macierz sąsiedztwa:\n");
        for (int i = 0; i < n; i++) {
            fprintf(fout, "[");
            for (int j = 0; j < n; j++) {
                fprintf(fout, "%d", matrix[i][j]);
                if (j < n - 1) fprintf(fout, " ");
            }
            fprintf(fout, "]\n");
        }
        
        fprintf(fout, "\nLista połączeń:\n");
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (matrix[i][j]) {
                    fprintf(fout, "%d - %d\n", i, j);
                }
            }
        }
        
        fclose(fin);



        if (matrix == NULL) {
            fprintf(stderr, "Blad odczytu macierzy sasiedztwa z pliku.\n");
            fclose(fout);
            return 1;
        }

        divide_into_balanced_groups(matrix, n, fout);





        for (int i = 0; i < n; i++) {
            free(matrix[i]);

        }
        free(matrix);
        fclose(fout);

        printf("\nOptymalizacja zakonczona. Wynik zapisano w: %s\n", output_file);
        return 0;
    }
    else {
        fprintf(stderr, "\nBlad! Niepoprawna liczba argumentow.\n");
        fprintf(stderr, "Dostepne opcje:\n");
        fprintf(stderr, "1. Generowanie: %s generate liczba_wierzcholkow liczba_krawedzi plik_wyjsciowy.txt\n", argv[0]);
        fprintf(stderr, "2. Optymalizacja: %s plik_wejsciowy.csrrg plik_wyjsciowy.txt\n\n", argv[0]);
        return 1;
    }
}