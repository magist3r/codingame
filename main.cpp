#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <chrono>

using namespace std;

static constexpr int ME = 1;
static constexpr int OPP = 0;
static constexpr int NONE = -1;

struct Tile {
    bool empty() { return x == -1 && y == -1; }
    int x = -1;
    int y = -1;
    int scrap_amount = 0;
    int owner = NONE;
    int units = 0;
    bool recycler, can_build, can_spawn, in_range_of_recycler, grass;
    ostream& dump(ostream& ioOut) const {
        ioOut << x << " " << y;
        return ioOut;
    }
    bool operator<(Tile other) const {
        return this->x < other.x;
    }
    bool operator==(Tile other) const {
        return this->x == other.x && this->y == other.y;
    }
    bool has_enemy_units() const {
        return this->owner == OPP && this->units > 0;
    }
};
// class for hash function
class MyHashFunction {
public:
    size_t operator()(const Tile& t) const
    {
        return t.x + t.y;
    }
};

ostream& operator<<(ostream& ioOut, const Tile& obj) { return obj.dump(ioOut); }

class Board
{
public:
    void addTile(Tile tile) {
        m_tiles[tile.x].emplace(tile.y, tile);
    }
    float distance(Tile t1, Tile t2, const vector<Tile> & additional = vector<Tile>()) const {
        int dist = abs(t2.x - t1.x) + abs(t2.y - t1.y);
        if (!additional.empty()) {
            int closest = max_distance();
            for (auto tile : additional) {
                int d = distance(t2, tile);
                if (d < closest)
                    closest = d;
            }
            return closest * 1. / max_distance() + dist;
        }

        return dist;
    }
    int number(Tile t) const {
        return t.x * width + t.y;
    }
    Tile tile(int x, int y) const {
        if (x < 0 || x > width - 1 || y < 0 || y > height - 1) return Tile();
        return m_tiles.at(x).at(y);
    }
    Tile * tilePtr(int x, int y) {
        if (x < 0 || x > width - 1 || y < 0 || y > height - 1) return nullptr;
        return &m_tiles.at(x).at(y);
    }
    Tile * tile_from_number(int number) {
        int x = number / width;
        int y = number % width;
        return tilePtr(x, y);
    }

    void clear() {
        m_tiles.clear();
    }

    int width;
    int height;
    int max_distance() const { return width + height; }
    int size() const { return width * height; }
private:
    map<int /*x*/, map<int /*y*/, Tile>> m_tiles;
};


int main()
{
    Board board;

    cin >> board.width >> board.height; cin.ignore();

    // game loop
    while (1) {
        auto start = std::chrono::steady_clock::now();
        board.clear();

        vector<Tile> my_tiles;
        vector<Tile> opp_tiles;
        vector<Tile> neutral_tiles;
        vector<Tile> my_units;
        vector<Tile> opp_units;
        vector<Tile> my_recyclers;
        vector<Tile> opp_recyclers;

        int my_matter;
        int opp_matter;
        cin >> my_matter >> opp_matter; cin.ignore();
        for (int y = 0; y < board.height; y++) {
            for (int x = 0; x < board.width; x++) {
                int scrap_amount;
                int owner; // 1 = me, 0 = foe, -1 = neutral
                int units;
                int recycler;
                int can_build;
                int can_spawn;
                int in_range_of_recycler;
                cin >> scrap_amount >> owner >> units >> recycler >> can_build >> can_spawn >> in_range_of_recycler; cin.ignore();

                Tile tile {x, y, scrap_amount, owner, units, recycler == 1, can_build == 1, can_spawn == 1, in_range_of_recycler == 1, scrap_amount == 0};
                board.addTile(tile);

                if (tile.owner == ME) {
                    my_tiles.emplace_back(tile);
                    if (tile.units > 0) {
                        my_units.emplace_back(tile);
                    } else if (tile.recycler) {
                        my_recyclers.emplace_back(tile);
                    }
                } else if (tile.owner == OPP) {
                    opp_tiles.emplace_back(tile);
                    if (tile.units > 0) {
                        opp_units.emplace_back(tile);
                    } else if (tile.recycler) {
                        opp_recyclers.emplace_back(tile);
                    }
                } else if (tile.scrap_amount > 0) {
                    neutral_tiles.emplace_back(tile);
                }
            }
        }

        vector<string> actions;

        auto tiles_togo_count = [&board, &opp_units](Tile tile) -> float {
            vector<pair<int, int>> next_coords;
            next_coords.emplace_back(tile.x - 1, tile.y);
            next_coords.emplace_back(tile.x + 1, tile.y);
            next_coords.emplace_back(tile.x, tile.y - 1);
            next_coords.emplace_back(tile.x, tile.y + 1);

            int count = 0;
            for (auto coord : next_coords) {
                if (coord.first >= board.width || coord.first < 0 || coord.second >= board.height || coord.second < 0) continue;

                Tile t = board.tile(coord.first, coord.second);

                if (!t.grass &&
                    ((t.owner == OPP && !t.recycler)
                    || t.owner == NONE)) {
                    ++count;
                }
            }
            return count + 1 - board.distance(tile, tile, opp_units);
        };

        auto tiles_scrap_count = [&board](Tile tile){
            vector<pair<int, int>> next_coords;
            next_coords.emplace_back(tile.x, tile.y);
            next_coords.emplace_back(tile.x - 1, tile.y);
            next_coords.emplace_back(tile.x + 1, tile.y);
            next_coords.emplace_back(tile.x, tile.y - 1);
            next_coords.emplace_back(tile.x, tile.y + 1);

            int scrap = 0;
            for (auto coord : next_coords) {
                if (coord.first >= board.width || coord.first < 0 || coord.second >= board.height || coord.second < 0) continue;

                Tile t = board.tile(coord.first, coord.second);

                if (t.in_range_of_recycler) { // break if any tile is in range of recycler
                    cerr << "in range " << t << endl;
                    return 0;
                }

                if (!t.recycler) {
                    scrap += t.scrap_amount;
                }
            }
            return scrap;
        };

        auto tiles_enemy_count = [&board](Tile tile) -> Tile {
            vector<Tile> to_check;

            auto index = [](int i) {
                if (i > 3) return i - 4;
                else if (i < 0) return i + 4;
                else return i;
            };

            to_check.emplace_back(board.tile(tile.x - 1, tile.y));
            to_check.emplace_back(board.tile(tile.x, tile.y - 1));
            to_check.emplace_back(board.tile(tile.x + 1, tile.y));
            to_check.emplace_back(board.tile(tile.x, tile.y + 1));

            bool has_enemy_units = false;
            bool has_enemy_units_close = false;
            for (int i = 0; i < to_check.size(); ++i) {
                Tile t = to_check.at(i);
                Tile t_prev = to_check.at(index(i-1));
                if (t.has_enemy_units()) has_enemy_units = true;
                if (t_prev.has_enemy_units() && t.has_enemy_units()) return {};
            }

            if (!has_enemy_units) return {};

            for (int i = 0; i < to_check.size(); ++i) {
                Tile t = to_check.at(i);
                Tile t_2 = to_check.at(index(i+2));
                if (t.has_enemy_units() && t_2.owner == ME) return t;
            }

            return {};
        };

        multimap<float, Tile> to_spawn;
        multimap<int, pair<Tile /*target*/, Tile /*enemy*/>> to_build;
        for (auto tile : my_tiles) {
            if (tile.can_spawn && !(tile.in_range_of_recycler && tile.scrap_amount == 1))
                to_spawn.emplace(tiles_togo_count(tile), tile);

            if (tile.can_build && !(tile.in_range_of_recycler && tile.scrap_amount == 1)) {
                Tile enemy_tile = tiles_enemy_count(tile);
                if (!enemy_tile.empty())
                    to_build.emplace(enemy_tile.units, make_pair(tile, enemy_tile));
            }
        }

        unordered_set<int> enemies;
        if (my_matter >= 20 && !to_build.empty()) {
            int enemy = -1;
            for (auto it = to_build.lower_bound(to_build.rbegin()->first); it != to_build.end(); it++) {
                int enemy_number = board.number(it->second.second);
                enemy = enemy_number;
                if (enemies.find(enemy_number) != enemies.end()) { // Found second enemy number
                    enemy = enemy_number;
                    break;
                } else
                    enemies.emplace(enemy_number);
            }

            if (enemy >= 0) {
                for (auto it = to_build.lower_bound(to_build.rbegin()->first); it != to_build.end(); it++) {
                    if (board.number(it->second.second) == enemy) {
                        Tile tile = it->second.first;
                        if (tile.can_build) {
                            ostringstream action;
                            action << "BUILD " << tile;
                            actions.emplace_back(action.str());
                            my_matter -= 10;
                        }
                    }
                }
            }
        }

        for (auto it = to_spawn.rbegin(); it != to_spawn.rend(); it++) {
            Tile tile = it->second;
            if (my_matter >= 20) {
                ostringstream action;
                action << "SPAWN " << 1 << " " << tile;
                actions.emplace_back(action.str());
                my_matter -= 10;
            }
            else
                break;
        }

        multimap<float /* distance */, pair<int /*source*/, int /* target*/>> to_move;
        for (Tile tile : my_units) {
            int dist_enemy = board.max_distance();
            Tile closest_enemy;
            for (auto t : opp_tiles) {
                if (tile.in_range_of_recycler && tile.scrap_amount == 1 || t.recycler) continue;
                to_move.emplace(board.distance(tile, t), make_pair(board.number(tile), board.number(t)));
            }

            int dist_neutral = board.max_distance();
            Tile closest_neutral;
            for (auto t : neutral_tiles) {
                if (tile.in_range_of_recycler && tile.scrap_amount == 1 || t.recycler) continue;
                to_move.emplace(board.distance(tile, t, opp_units), make_pair(board.number(tile), board.number(t)));
            }
        }

        unordered_set<int> used_tiles;
        for (auto it = to_move.begin(); it != to_move.end(); it++) {
            Tile * source = board.tile_from_number(it->second.first);
            Tile * target = board.tile_from_number(it->second.second);

            if (source && target && used_tiles.find(it->second.second) == used_tiles.end() && source->units > 0) {
                int amount = 1;
                source->units -= amount;
                ostringstream action;
                action << "MOVE " << amount << " " << *source << " " << *target;
                actions.emplace_back(action.str());
                used_tiles.insert(it->second.second);
            }
        }

        if (actions.empty()) {
             cout << "WAIT" << endl;
        } else {
            for (const auto & action : actions) {
                cout << action << ";";
            }
            cout << endl;
        }
    }
}
