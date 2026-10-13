#include "Board.h"
#include "Move.h"
#include "Types.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

struct MoveRecord {
    std::string desde;
    std::string hasta;
};

struct GameRecord {
    std::string id;
    std::string winner; // "p1" or "p2"
    std::vector<MoveRecord> moves;
};

void process_game(const GameRecord& game, std::mt19937& rng, std::ofstream& out,
                  int& processed_count, int& skipped_count) {
    // Filter: skip games without clear winner ("p1" or "p2") or with fewer than 2 moves
    if ((game.winner != "p1" && game.winner != "p2") || game.moves.size() < 2) {
        skipped_count++;
        return;
    }

    // Sample an intermediate position uniformly in ply k in [1, N - 1]
    // k = 1 is the state after move 1; k = N - 1 is the state before the final move N.
    std::uniform_int_distribution<size_t> dist(1, game.moves.size() - 1);
    size_t k = dist(rng);

    Board b;
    for (size_t i = 0; i < k; ++i) {
        uint8_t from = string_to_square(game.moves[i].desde);
        uint8_t to = string_to_square(game.moves[i].hasta);
        b.apply_move(Move(from, to));
    }

    // Extract differential features: (SideToMove - Opponent)
    Board::Features feats = b.get_differential_features();

    // Expected value: +500 if SideToMove won, -500 if SideToMove lost
    Color winner_color = (game.winner == "p1") ? Color::BLACK : Color::WHITE;
    int result = (winner_color == b.turn()) ? 500 : -500;

    out << feats.avg_dist << ","
        << feats.adjacencies << ","
        << feats.count << ","
        << result << "\n";

    processed_count++;
}

int main(int argc, char* argv[]) {
    std::string input_path = "Base de Datos.csv";
    std::string output_path = "features.csv";
    uint32_t seed = 42;

    if (argc > 1) {
        input_path = argv[1];
    }
    if (argc > 2) {
        output_path = argv[2];
    }
    if (argc > 3) {
        seed = static_cast<uint32_t>(std::stoul(argv[3]));
    }

    std::cout << "Input dataset:  " << input_path << "\n";
    std::cout << "Output file:    " << output_path << "\n";
    std::cout << "Random seed:    " << seed << "\n";

    std::ifstream in(input_path);
    if (!in.is_open()) {
        std::cerr << "Error: unable to open input file '" << input_path << "'\n";
        return 1;
    }

    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "Error: unable to open output file '" << output_path << "'\n";
        return 1;
    }

    // Header with exactly 4 columns
    out << "avg_dist,adjacencies,count,result\n";

    std::mt19937 rng(seed);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::string line;
    // Skip header line in CSV
    if (!std::getline(in, line)) {
        std::cerr << "Error: input file is empty.\n";
        return 1;
    }

    GameRecord current_game;
    int total_games = 0;
    int processed_games = 0;
    int skipped_games = 0;

    auto flush_game = [&]() {
        if (!current_game.id.empty()) {
            total_games++;
            process_game(current_game, rng, out, processed_games, skipped_games);
            current_game.moves.clear();
            current_game.id.clear();
            current_game.winner.clear();
        }
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        // Fast semicolon tokenization:
        // Format: id_partida;n_jugada;jugador;nombre;desde;hasta;captura;ganador;nombre_ganador
        std::string tokens[9];
        int token_idx = 0;
        size_t start = 0;
        for (size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == ';') {
                if (token_idx < 9) {
                    tokens[token_idx++] = line.substr(start, i - start);
                }
                start = i + 1;
            }
        }

        if (token_idx < 8) continue;

        const std::string& gid = tokens[0];
        const std::string& desde = tokens[4];
        const std::string& hasta = tokens[5];
        const std::string& ganador = tokens[7];

        if (gid != current_game.id) {
            flush_game();
            current_game.id = gid;
            current_game.winner = ganador;
        }

        current_game.moves.push_back({desde, hasta});
    }
    flush_game();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "\nExtraction finished in " << elapsed.count() << " seconds.\n";
    std::cout << "Total matches in DB:  " << total_games << "\n";
    std::cout << "Matches processed:    " << processed_games << "\n";
    std::cout << "Matches skipped:      " << skipped_games << " (draws or < 2 moves)\n";
    std::cout << "Generated features:   " << output_path << "\n";

    return 0;
}
