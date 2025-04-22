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

void generate_random_graph(int n, int edges, const char *output_filename) {
    // Inicjalizacja generatora liczb losowych
    srand(time(NULL));
    
    // Otwarcie pliku do zapisu
    FILE *file = fopen(output_filename, "w");
    if (file == NULL) {
        printf("Nie można otworzyć pliku do zapisu.\n");
        return;
    }
    
    // Inicjalizacja macierzy sąsiedztwa zerami
    int **adj_matrix = (int **)malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        adj_matrix[i] = (int *)calloc(n, sizeof(int));
    }
    
    // Generowanie losowych krawędzi
    int edges_generated = 0;
    while (edges_generated < edges) {
        int from = rand() % n;
        int to = rand() % n;
        
        // Sprawdzamy czy krawędź już nie istnieje i nie jest pętlą (from != to)
        if (from != to && adj_matrix[from][to] == 0) {
            adj_matrix[from][to] = 1;
            edges_generated++;
        }
    }
    
    // Zapis macierzy sąsiedztwa do pliku
    fprintf(file, "Graf:\n");
    fprintf(file, "Macierz sąsiedztwa:\n");
    for (int i = 0; i < n; i++) {
        fprintf(file, "[");
        for (int j = 0; j < n; j++) {
            fprintf(file, "%d.", adj_matrix[i][j]);
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
    
    // Zamknięcie pliku i zwolnienie pamięci
    fclose(file);
    for (int i = 0; i < n; i++) {
        free(adj_matrix[i]);
    }
    free(adj_matrix);
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
void divide_into_balanced_groups(int **matrix, int n, FILE *fout) {
    if (n < 3) {
        fprintf(fout, "Za mało wierzchołków (min. 3).\n");
        return;
    }

    int *best_groups = malloc(n * sizeof(int));
    int min_cross = INT_MAX;
    
    // Parametry algorytmu
    const int max_attempts = 10;  // Liczba prób z różnymi inicjalizacjami
    const double initial_temp = 10.0;
    const double cooling_rate = 0.95;
    
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        int *groups = malloc(n * sizeof(int));
        
        // Inicjalizacja - lepsze podejście niż proste modulo
        if (attempt == 0) {
            // Pierwsza próba - równomierny podział
            for (int i = 0; i < n; i++) groups[i] = i % 3;
        } else {
            // Kolejne próby - losowa inicjalizacja z zachowaniem równowagi
            int target_size = n / 3;
            for (int i = 0; i < n; i++) groups[i] = -1;
            
            for (int g = 0; g < 3; g++) {
                int count = 0;
                while (count < target_size) {
                    int idx = rand() % n;
                    if (groups[idx] == -1) {
                        groups[idx] = g;
                        count++;
                    }
                }
            }
            
            // Przypisz pozostałe wierzchołki
            for (int i = 0; i < n; i++) {
                if (groups[i] == -1) groups[i] = rand() % 3;
            }
        }
        
        int current_cross = calculate_cross_connections(matrix, n, groups);
        
        // Symulowane wyżarzanie
        double temp = initial_temp;
        while (temp > 0.1) {
            int improved = 0;
            
            for (int i = 0; i < n; i++) {
                int original_group = groups[i];
                
                // Sprawdź wszystkie możliwe przemieszczenia
                for (int g = 0; g < 3; g++) {
                    if (g == original_group) continue;
                    
                    // Sprawdź ograniczenia równowagi
                    int new_group_sizes[3] = {0};
                    for (int j = 0; j < n; j++) {
                        int group = (j == i) ? g : groups[j];
                        new_group_sizes[group]++;
                    }
                    
                    int max_size = (n + 2) / 3;  // ceil(n/3)
                    int min_size = (n - 1) / 3;   // floor(n/3)
                    
                    if (new_group_sizes[g] > max_size || new_group_sizes[original_group] < min_size) {
                        continue;
                    }
                    
                    groups[i] = g;
                    int new_cross = calculate_cross_connections(matrix, n, groups);
                    
                    // Kryterium akceptacji
                    if (new_cross < current_cross || 
                        (temp > 0 && exp((current_cross - new_cross)/temp) > (double)rand()/RAND_MAX)) {
                        
                        current_cross = new_cross;
                        improved = 1;
                        
                        if (current_cross < min_cross) {
                            min_cross = current_cross;
                            memcpy(best_groups, groups, n * sizeof(int));
                        }
                    } else {
                        groups[i] = original_group;
                    }
                }
            }
            
            // Schładzanie
            temp *= cooling_rate;
            
            if (!improved && temp < 0.5) break;
        }
        
        free(groups);
    }
    
    // Wynik
    fprintf(fout, "\nOptymalny podział na 3 grupy:\n");
    fprintf(fout, "Minimalna liczba połączeń między grupami: %d\n", min_cross);
    
    int counts[3] = {0};
    for (int i = 0; i < n; i++) counts[best_groups[i]]++;
    
    fprintf(fout, "Liczebność grup: %d, %d, %d\n", counts[0], counts[1], counts[2]);
    
    for (int g = 0; g < 3; g++) {
        fprintf(fout, "Grupa %d: ", g);
        for (int i = 0; i < n; i++) {
            if (best_groups[i] == g) fprintf(fout, "%d ", i);
        }
        fprintf(fout, "\n");
    }
    
    // Dodatkowa analiza
    fprintf(fout, "\nAnaliza połączeń:\n");
    for (int g1 = 0; g1 < 3; g1++) {
        for (int g2 = g1+1; g2 < 3; g2++) {
            int connections = 0;
            for (int i = 0; i < n; i++) {
                if (best_groups[i] != g1) continue;
                for (int j = 0; j < n; j++) {
                    if (best_groups[j] == g2 && matrix[i][j]) {
                        connections++;
                    }
                }
            }
            fprintf(fout, "Połączenia między grupą %d a %d: %d\n", g1, g2, connections);
        }
    }
    
    free(best_groups);
}
int** read_adjacency_matrix(FILE *fin, int *n) {
    // Skip first line (50)
    char line[MAX_LINE_LENGTH];
    fgets(line, MAX_LINE_LENGTH, fin);
    
    // Read edges list
    fgets(line, MAX_LINE_LENGTH, fin);
    int edge_count;
    int *edges = parse_int_array(line, &edge_count);
    
    // Read indices
    fgets(line, MAX_LINE_LENGTH, fin);
    int index_count;
    int *indices = parse_int_array(line, &index_count);
    
    // Determine number of vertices (n)
    *n = indices[index_count-1] / 2; // Rough estimate
    
    // Create empty matrix
    int **matrix = malloc(*n * sizeof(int*));
    for(int i = 0; i < *n; i++) {
        matrix[i] = calloc(*n, sizeof(int));
    }
    
    // Populate matrix from edges
    for(int i = 0; i < edge_count; i++) {
        int v1 = edges[i] / *n;
        int v2 = edges[i] % *n;
        if(v1 < *n && v2 < *n) {
            matrix[v1][v2] = 1;
            matrix[v2][v1] = 1;
        }
    }
    
    free(edges);
    free(indices);
    return matrix;
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