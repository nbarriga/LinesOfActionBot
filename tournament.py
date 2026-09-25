#!/usr/bin/env python3
"""
tournament.py - Lines of Action UCI Bot Match & Tournament Runner

Plays batch matches between two UCI-compliant bots (or the same bot with different
options/algorithms), alternating colors to eliminate first-player advantage,
and calculates win/loss/draw statistics and Elo ratings.

Expected UCI Protocol Commands:
  - uci                   : Handshake; bot responds with id/options and 'uciok'
  - isready               : Sync ping; bot responds with 'readyok'
  - ucinewgame            : Resets engine state / transposition table for a new game
  - setoption name ...    : Sets engine configuration (Algorithm, Depth, UseTT, OrderMoves)
  - position startpos moves ... : Sets up the board position and applies moves
  - go [depth N] [wtime W btime B winc I binc I] [movetime M] :
                            Starts search; bot responds with 'bestmove <move>' or
                            'info string gameover <winner>_wins' + 'bestmove (none)'
  - legalmoves            : Returns space-separated legal moves (needed for --random-moves)
  - quit                  : Exits the engine process cleanly

Usage Examples:
  # Pit AlphaBeta with TT against AlphaBeta without TT for 10 games:
  python3 tournament.py --games 10 \
    --bot1-args "--algo alphabeta --tt true" \
    --bot2-args "--algo alphabeta --tt false"

  # Pit depth 4 against depth 3 with 2 random opening moves:
  python3 tournament.py --games 10 --random-moves 2 \
    --bot1-args "--depth 4" \
    --bot2-args "--depth 3"

  # Use UCI setoption commands:
  python3 tournament.py --games 6 \
    --bot1-opt "UseTT=true" \
    --bot2-opt "UseTT=false"
"""

import argparse
import json
import math
import os
import random
import subprocess
import sys
import time
from typing import Dict, List, Optional, Tuple


class UCIEngine:
    def __init__(self, command: str, name: str, uci_options: Optional[List[str]] = None):
        self.command = command
        self.name = name
        self.uci_options = uci_options or []
        self.process: Optional[subprocess.Popen] = None

    def start(self):
        self.process = subprocess.Popen(
            self.command,
            shell=True,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self.send_command("uci")
        while True:
            line = self.read_line()
            if line is None or line.strip() == "uciok":
                break

        # Send custom UCI options
        for opt in self.uci_options:
            if "=" in opt:
                opt_name, opt_val = opt.split("=", 1)
                self.send_command(f"setoption name {opt_name.strip()} value {opt_val.strip()}")
            else:
                self.send_command(f"setoption name {opt.strip()}")

        self.send_command("isready")
        while True:
            line = self.read_line()
            if line is None or line.strip() == "readyok":
                break

    def new_game(self):
        self.send_command("ucinewgame")
        self.send_command("isready")
        while True:
            line = self.read_line()
            if line is None or line.strip() == "readyok":
                break

    def send_command(self, cmd: str):
        if self.process and self.process.stdin:
            self.process.stdin.write(cmd + "\n")
            self.process.stdin.flush()

    def read_line(self, timeout: float = 30.0) -> Optional[str]:
        if not self.process or not self.process.stdout:
            return None
        return self.process.stdout.readline()

    def get_move(
        self,
        position_cmd: str,
        depth: Optional[int] = None,
        movetime: Optional[int] = None,
        wtime: Optional[int] = None,
        btime: Optional[int] = None,
        winc: Optional[int] = None,
        binc: Optional[int] = None,
        timeout: float = 60.0,
    ) -> Tuple[Optional[str], Optional[str]]:
        """
        Sends position and go, returns (bestmove, gameover_message).
        """
        self.send_command(position_cmd)
        go_parts = ["go"]
        if depth:
            go_parts.append(f"depth {depth}")
        if movetime:
            go_parts.append(f"movetime {movetime}")
        if wtime is not None:
            go_parts.append(f"wtime {wtime}")
        if btime is not None:
            go_parts.append(f"btime {btime}")
        if winc is not None:
            go_parts.append(f"winc {winc}")
        if binc is not None:
            go_parts.append(f"binc {binc}")
        self.send_command(" ".join(go_parts))

        bestmove = None
        gameover_msg = None

        start_time = time.time()
        while time.time() - start_time < timeout:
            line = self.read_line(timeout=timeout)
            if line is None:
                break
            line = line.strip()
            if line.startswith("info string gameover"):
                gameover_msg = line.replace("info string gameover", "").strip()
            elif line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    bestmove = parts[1]
                break

        return bestmove, gameover_msg

    def get_legal_moves(self, position_cmd: str, timeout: float = 10.0) -> List[str]:
        self.send_command(position_cmd)
        self.send_command("legalmoves")
        start_time = time.time()
        while time.time() - start_time < timeout:
            line = self.read_line(timeout=timeout)
            if line is None:
                break
            line = line.strip()
            if line.startswith("legalmoves"):
                parts = line.split()
                return parts[1:]
        return []

    def stop(self):
        if self.process:
            try:
                self.send_command("quit")
                self.process.terminate()
                self.process.wait(timeout=2.0)
            except Exception:
                self.process.kill()
            self.process = None


def calculate_elo(score: float) -> str:
    """Calculates Elo difference given score (fraction between 0 and 1)."""
    if score >= 0.999:
        return "+inf"
    if score <= 0.001:
        return "-inf"
    elo_diff = -400.0 * math.log10((1.0 / score) - 1.0)
    return f"{elo_diff:+.1f}"


def generate_random_opening(bot: UCIEngine, num_moves: int, max_retries: int = 10) -> List[str]:
    """Generates num_moves random legal moves from startpos, ensuring game doesn't end during opening."""
    for _ in range(max_retries):
        moves: List[str] = []
        valid = True
        for _ in range(num_moves):
            pos_cmd = "position startpos"
            if moves:
                pos_cmd += " moves " + " ".join(moves)
            legal = bot.get_legal_moves(pos_cmd)
            if not legal:
                valid = False
                break
            chosen = random.choice(legal)
            moves.append(chosen)

        if valid and len(moves) == num_moves:
            check_cmd = "position startpos moves " + " ".join(moves)
            remaining_legal = bot.get_legal_moves(check_cmd)
            if remaining_legal:
                return moves
    return moves


def play_game(
    black_bot: UCIEngine,
    white_bot: UCIEngine,
    max_plies: int = 60,
    depth: Optional[int] = None,
    movetime: Optional[int] = None,
    base_time: Optional[int] = None,
    inc_time: Optional[int] = None,
    opening_moves: Optional[List[str]] = None,
    verbose: bool = False,
) -> Dict:
    black_bot.new_game()
    white_bot.new_game()

    moves: List[str] = list(opening_moves or [])
    winner = None
    reason = None
    gameover_msg = None

    btime = base_time
    wtime = base_time
    inc = inc_time or 0

    ply = len(moves) + 1

    while ply <= max_plies:
        is_black_turn = (ply % 2 == 1)
        current_bot = black_bot if is_black_turn else white_bot
        player_color = "black" if is_black_turn else "white"

        if moves:
            pos_cmd = "position startpos moves " + " ".join(moves)
        else:
            pos_cmd = "position startpos"

        t_start = time.time()
        bestmove, go_msg = current_bot.get_move(
            pos_cmd,
            depth=depth,
            movetime=movetime,
            wtime=wtime,
            btime=btime,
            winc=inc if base_time is not None else None,
            binc=inc if base_time is not None else None,
        )
        t_spent_ms = int((time.time() - t_start) * 1000)

        if base_time is not None:
            if is_black_turn:
                if t_spent_ms > btime and not winner:
                    winner = "white"
                    reason = "Black flagged (ran out of time)"
                btime = max(0, btime - t_spent_ms) + inc
            else:
                if t_spent_ms > wtime and not winner:
                    winner = "black"
                    reason = "White flagged (ran out of time)"
                wtime = max(0, wtime - t_spent_ms) + inc

        if go_msg:
            gameover_msg = go_msg

        if not bestmove or bestmove in ("(none)", "none", "resign"):
            if gameover_msg:
                if "black" in gameover_msg:
                    winner = "black"
                    reason = "Black won (connection detected)"
                elif "white" in gameover_msg:
                    winner = "white"
                    reason = "White won (connection detected)"
                else:
                    winner = "draw"
                    reason = gameover_msg
            elif not winner:
                # Resignation or crash
                winner = "white" if is_black_turn else "black"
                reason = f"{player_color.capitalize()} resigned or returned no move"
            break

        moves.append(bestmove)
        if verbose:
            move_num = (ply + 1) // 2
            clk_str = f" [B:{btime}ms W:{wtime}ms]" if base_time is not None else ""
            print(f"  Ply {ply:2d} ({player_color[:1].upper()}): {bestmove} ({t_spent_ms}ms){clk_str}")

        if winner:
            break

        ply += 1

    if ply > max_plies and winner is None:
        winner = "draw"
        reason = f"Max plies reached ({max_plies})"

    return {
        "winner": winner,
        "reason": reason,
        "plies": len(moves),
        "moves": moves,
        "opening_moves": opening_moves or [],
    }


def main():
    uci_help = """
Expected UCI Protocol Commands:
  The bot is expected to communicate via standard I/O using the Universal Chess
  Interface (UCI) protocol adapted for Lines of Action. The tournament runner relies on:

  uci
    Handshake initialization. Bot responds with identification and supported options:
      id name <Engine Name>
      id author <Author Name>
      option name <Name> type <check|spin|combo|string> ...
      uciok

  isready
    Synchronization ping. Engine must respond with:
      readyok

  ucinewgame
    Signals the start of a new game. The engine clears its transposition table
    and internal search history. Typically followed by 'isready'.

  setoption name <Name> [value <Value>]
    Configures an engine option (passed via --bot1-opt / --bot2-opt). Supported:
      Algorithm  : Search algorithm ('alphabeta', 'negamax')
      Depth      : Default search depth (integer >= 1)
      UseTT      : Enable/disable transposition table ('true', 'false')
      OrderMoves : Enable/disable move ordering ('true', 'false')

  position startpos [moves <m1> <m2> ...]
    Sets current board position (initial setup) and applies moves (e.g. 'b1d3 a2c4').

  go [depth <N>] [wtime <W> btime <B> winc <wI> binc <bI>] [movetime <M>]
    Starts search from current position. Engine responds with:
      bestmove <move>                     (e.g. 'bestmove b1d3')
    If game is over or engine resigns (no legal moves / terminal state):
      info string gameover <winner>_wins  (e.g. 'black_wins' or 'white_wins')
      bestmove (none)

  legalmoves
    Returns space-separated list of all legal moves for current board:
      legalmoves <m1> <m2> ...            (e.g. 'legalmoves b1d3 b1b3 b1b4')
    If game is over, engine returns 'legalmoves' (empty list).
    Required by tournament.py when generating random opening moves (--random-moves).

  quit
    Instructs the bot to exit cleanly.
"""

    parser = argparse.ArgumentParser(
        description="Run UCI Lines of Action bot matches and tournaments.",
        epilog=uci_help,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--bot1", default="./loabot", help="Path or command for Bot 1 (default: ./loabot)")
    parser.add_argument("--bot2", default="./loabot", help="Path or command for Bot 2 (default: ./loabot)")
    parser.add_argument("--bot1-name", default="Bot1", help="Display name for Bot 1")
    parser.add_argument("--bot2-name", default="Bot2", help="Display name for Bot 2")
    parser.add_argument("--bot1-args", default="", help="Command-line flags to pass to Bot 1 (e.g. '--algo alphabeta --tt false')")
    parser.add_argument("--bot2-args", default="", help="Command-line flags to pass to Bot 2")
    parser.add_argument("--bot1-opt", action="append", default=[], help="UCI option for Bot 1 (e.g. 'UseTT=false')")
    parser.add_argument("--bot2-opt", action="append", default=[], help="UCI option for Bot 2")
    parser.add_argument("--games", type=int, default=10, help="Total number of games to play (paired, default: 10)")
    parser.add_argument("--depth", type=int, default=None, help="Search depth override passed to 'go depth N'")
    parser.add_argument("--movetime", type=int, default=None, help="Fixed move time in ms passed to 'go movetime M'")
    parser.add_argument("--time", type=int, default=None, help="Initial clock time in ms (e.g. 180000 for 3m)")
    parser.add_argument("--inc", type=int, default=0, help="Time increment in ms per move (e.g. 2000 for 2s)")
    parser.add_argument("--max-plies", type=int, default=60, help="Maximum plies before declaring a draw (default: 60)")
    parser.add_argument("--random-moves", "--random-plies", type=int, default=0,
                        help="Number of random moves to play first. The same moves are used for each paired match.")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for opening moves reproducibility")
    parser.add_argument("--json", type=str, default=None, help="Save tournament results to a JSON file")
    parser.add_argument("--verbose", "-v", action="store_true", help="Print each move as it is played")

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    # Build full shell commands
    cmd1 = f"{args.bot1} {args.bot1_args}".strip()
    cmd2 = f"{args.bot2} {args.bot2_args}".strip()

    name1 = args.bot1_name
    if args.bot1_args:
        name1 += f" ({args.bot1_args})"
    name2 = args.bot2_name
    if args.bot2_args:
        name2 += f" ({args.bot2_args})"

    print("=" * 65)
    print("      Lines of Action Bot Tournament & Match Runner       ")
    print("=" * 65)
    print(f"Bot 1 : {name1}")
    print(f"        Cmd: {cmd1}")
    if args.bot1_opt:
        print(f"        UCI: {args.bot1_opt}")
    print(f"Bot 2 : {name2}")
    print(f"        Cmd: {cmd2}")
    if args.bot2_opt:
        print(f"        UCI: {args.bot2_opt}")
    print(f"Games : {args.games} (paired matches)")
    if args.random_moves > 0:
        print(f"Opening : {args.random_moves} random moves per pair" + (f" (seed: {args.seed})" if args.seed is not None else ""))
    print(f"Plies : max {args.max_plies}")
    print("=" * 65)

    bot1 = UCIEngine(cmd1, name1, args.bot1_opt)
    bot2 = UCIEngine(cmd2, name2, args.bot2_opt)

    try:
        bot1.start()
        bot2.start()
    except Exception as e:
        print(f"Error starting bots: {e}", file=sys.stderr)
        sys.exit(1)

    # Simple opening seeds (symmetrical or opening book plies)
    openings = [
        [],  # standard startpos
        ["b1d3", "a2c4"],
        ["g1e3", "h2f4"],
        ["b8d6", "a7c5"],
        ["g8e6", "h7f5"],
    ]

    bot1_wins = 0
    bot2_wins = 0
    draws = 0
    game_records = []

    start_tournament_time = time.time()

    # Games are played in pairs: 
    # Game 2i:   Bot 1 is Black, Bot 2 is White
    # Game 2i+1: Bot 2 is Black, Bot 1 is White
    num_pairs = (args.games + 1) // 2

    game_idx = 1
    for pair_idx in range(num_pairs):
        if args.random_moves > 0:
            opening = generate_random_opening(bot1, args.random_moves)
        else:
            opening = openings[pair_idx % len(openings)]
        op_str = " ".join(opening) if opening else "startpos"

        # Game A: Bot 1 = Black, Bot 2 = White
        if game_idx <= args.games:
            print(f"\nGame {game_idx}/{args.games}: [Black] {args.bot1_name} vs [White] {args.bot2_name} (opening: {op_str})")
            t0 = time.time()
            res = play_game(
                bot1, bot2,
                max_plies=args.max_plies,
                depth=args.depth,
                movetime=args.movetime,
                base_time=args.time,
                inc_time=args.inc,
                opening_moves=opening,
                verbose=args.verbose,
            )
            duration = time.time() - t0

            if res["winner"] == "black":
                bot1_wins += 1
                winner_name = args.bot1_name
            elif res["winner"] == "white":
                bot2_wins += 1
                winner_name = args.bot2_name
            else:
                draws += 1
                winner_name = "Draw"

            print(f"  Result: {winner_name} in {res['plies']} plies ({duration:.2f}s) - {res['reason']}")
            res.update({"game": game_idx, "black": args.bot1_name, "white": args.bot2_name, "duration": duration})
            game_records.append(res)
            game_idx += 1

        # Game B: Bot 2 = Black, Bot 1 = White
        if game_idx <= args.games:
            print(f"\nGame {game_idx}/{args.games}: [Black] {args.bot2_name} vs [White] {args.bot1_name} (opening: {op_str})")
            t0 = time.time()
            res = play_game(
                bot2, bot1,
                max_plies=args.max_plies,
                depth=args.depth,
                movetime=args.movetime,
                base_time=args.time,
                inc_time=args.inc,
                opening_moves=opening,
                verbose=args.verbose,
            )
            duration = time.time() - t0

            if res["winner"] == "black":
                bot2_wins += 1
                winner_name = args.bot2_name
            elif res["winner"] == "white":
                bot1_wins += 1
                winner_name = args.bot1_name
            else:
                draws += 1
                winner_name = "Draw"

            print(f"  Result: {winner_name} in {res['plies']} plies ({duration:.2f}s) - {res['reason']}")
            res.update({"game": game_idx, "black": args.bot2_name, "white": args.bot1_name, "duration": duration})
            game_records.append(res)
            game_idx += 1

    total_time = time.time() - start_tournament_time
    total_games = bot1_wins + bot2_wins + draws
    score1 = (bot1_wins + 0.5 * draws) / total_games if total_games > 0 else 0.5
    score2 = (bot2_wins + 0.5 * draws) / total_games if total_games > 0 else 0.5

    elo_diff1 = calculate_elo(score1)

    print("\n" + "=" * 65)
    print("                    FINAL TOURNAMENT RESULTS                     ")
    print("=" * 65)
    print(f"Total Games Played : {total_games}")
    print(f"Total Time Elapsed : {total_time:.2f}s")
    print("-" * 65)
    print(f"{name1:<35} : {bot1_wins} wins ({score1 * 100:.1f}%) [Elo: {elo_diff1}]")
    print(f"{name2:<35} : {bot2_wins} wins ({score2 * 100:.1f}%)")
    print(f"{'Draws':<35} : {draws} ({draws / total_games * 100:.1f}%)")
    print("=" * 65)

    bot1.stop()
    bot2.stop()

    if args.json:
        out_data = {
            "bot1": name1,
            "bot2": name2,
            "total_games": total_games,
            "bot1_wins": bot1_wins,
            "bot2_wins": bot2_wins,
            "draws": draws,
            "score_bot1": score1,
            "elo_diff": elo_diff1,
            "total_time_sec": total_time,
            "games": game_records,
        }
        with open(args.json, "w") as f:
            json.dump(out_data, f, indent=2)
        print(f"Results saved to {args.json}")


if __name__ == "__main__":
    main()
