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
    int distance(Tile t1, Tile t2, const vector<Tile> & additional = vector<Tile>()) const {
        return abs(t2.x - t1.x) + abs(t2.y - t1.y);
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
    int max_distance() { return width + height; }
    int size() { return width * height; }
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

        auto tiles_togo_count = [&board](Tile tile){
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
            return count;
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

        auto tiles_enemy_count = [&board](Tile tile){
            vector<pair<int, int>> next_coords;
            Tile t1 = board.tile(tile.x - 1, tile.y);
            Tile t2 = board.tile(tile.x, tile.y - 1);
            Tile t3 = board.tile(tile.x + 1, tile.y);
            Tile t4 = board.tile(tile.x, tile.y + 1);

            if (t1.owner == ME) {
                if (t2.owner == OPP && t2.units > 0) return 0;
                if (t4.owner == OPP && t4.units > 0) return 0;
                if (!(t3.owner == OPP && t3.units > 0)) return 0;
                return t3.units;
            }
            else if (t2.owner == ME) {
                if (t3.owner == OPP && t3.units > 0) return 0;
                if (t1.owner == OPP && t1.units > 0) return 0;
                if (!(t4.owner == OPP && t4.units > 0)) return 0;
                return t4.units;
            } else if (t3.owner == ME) {
                if (t2.owner == OPP && t2.units > 0) return 0;
                if (t4.owner == OPP && t4.units > 0) return 0;
                if (!(t1.owner == OPP && t1.units > 0)) return 0;
                return t1.units;
            } else if (t4.owner == ME) {
                if (t3.owner == OPP && t3.units > 0) return 0;
                if (t1.owner == OPP && t1.units > 0) return 0;
                if (!(t2.owner == OPP && t2.units > 0)) return 0;
                return t2.units;
            } else
                return 0;
        };

        multimap<int, Tile> to_spawn;
        multimap<int, Tile> to_build;
        for (auto tile : my_tiles) {
            if (tile.can_spawn)
                to_spawn.emplace(tiles_togo_count(tile), tile);

            if (tile.can_build)
                to_build.emplace(tiles_enemy_count(tile), tile);
        }

        if (my_matter >= 10) {
            for (auto it = to_build.rbegin(); it != to_build.rend(); it++) {
                if (it->first == 0) break;

                Tile tile = it->second;
                if (tile.can_build) {
                    ostringstream action;
                    action << "BUILD " << tile;
                    actions.emplace_back(action.str());
                    my_matter -= 10;
                    break;
                } else
                    continue;
            }
        }

        for (auto it = to_spawn.rbegin(); it != to_spawn.rend(); it++) {
            Tile tile = it->second;
            if (my_matter >= 10) {
                ostringstream action;
                action << "SPAWN " << 1 << " " << tile;
                actions.emplace_back(action.str());
                my_matter -= 10;
            }
            else
                break;
        }

        multimap<int /* distance */, pair<int /*source*/, int /* target*/>> to_move;
        for (Tile tile : my_units) {
            int dist_enemy = board.max_distance();
            Tile closest_enemy;
            for (auto t : opp_tiles) {
                to_move.emplace(board.distance(tile, t), make_pair(board.number(tile), board.number(t)));
            }

            int dist_neutral = board.max_distance();
            Tile closest_neutral;
            for (auto t : neutral_tiles) {
                to_move.emplace(board.distance(tile, t), make_pair(board.number(tile), board.number(t)));
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
