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

void generate_random_graph(int n, double p, const char *output_filename) {
    if (n <= 0) {
        fprintf(stderr, "Liczba wierzcholkow (polaczen) musi byc dodatnia.\n");
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

    fprintf(fout, "Macierz s\xC4\x85siedztwa przedstawiajaca dzialy w firmie (rozmieszczenie przestrzenne):\n");
    for (int i = 0; i < n; i++) {
        fprintf(fout, "[");
        for (int j = 0; j < n; j++) {
            fprintf(fout, "%d.", matrix[i][j]);
            if (j < n - 1) fprintf(fout, " ");
        }
        fprintf(fout, "]\n");
    }

    fprintf(fout, "\nLista polaczen komunikacyjnych miedzy numerami ID poszczegolnych dzialow:\n");
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

    divide_into_balanced_groups(matrix, n, fout);

    for (int i = 0; i < n; i++) {
        free(matrix[i]);
    }
    free(matrix);
    fclose(fout);

    printf("\n\n  Zoptymalizowano polaczenia komunikacyjne poszczegolnych dzialow firmy i zapisano w pliku %s.\n ", argv[2]);
    printf(" Teraz zarzadzanie Wasza firma bedzie przebiegalo sprawniej. \n  Nie zapomnijcie przypisac osoby nadzorujacej do nowo utworzonych dzialow!\n");
    
    return 0;
}