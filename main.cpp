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
    bool is_empty() { return x == -1 && y == -1; }
    int x = -1;
    int y = -1;
    int scrap_amount, owner, units;
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

int distance(Tile t1, Tile t2) {
    return abs(t2.x - t1.x) + abs(t2.y - t1.y);
}

int number(Tile t, int width) {
    return t.x * width + t.y;
}


int main()
{
    int width;
    int height;
    cin >> width >> height; cin.ignore();

    int max_distance = width + height;

    // game loop
    while (1) {
        auto start = std::chrono::steady_clock::now();

        vector<Tile> my_tiles;
        unordered_map<int, Tile> opp_tiles;
        unordered_map<int, Tile> neutral_tiles;
        vector<Tile> my_units;
        vector<Tile> opp_units;
        vector<Tile> my_recyclers;
        vector<Tile> opp_recyclers;

        map<int /*x*/, map<int /*y*/, Tile>> tiles;

        int my_matter;
        int opp_matter;
        cin >> my_matter >> opp_matter; cin.ignore();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int scrap_amount;
                int owner; // 1 = me, 0 = foe, -1 = neutral
                int units;
                int recycler;
                int can_build;
                int can_spawn;
                int in_range_of_recycler;
                cin >> scrap_amount >> owner >> units >> recycler >> can_build >> can_spawn >> in_range_of_recycler; cin.ignore();

                Tile tile = {x, y, scrap_amount, owner, units, recycler == 1, can_build == 1, can_spawn == 1, in_range_of_recycler == 1, scrap_amount == 0};
                tiles[x].emplace(y, tile);

                if (tile.owner == ME) {
                    my_tiles.emplace_back(tile);
                    if (tile.units > 0) {
                        my_units.emplace_back(tile);
                    } else if (tile.recycler) {
                        my_recyclers.emplace_back(tile);
                    }
                } else if (tile.owner == OPP) {
                    opp_tiles.emplace(x * width + y, tile);
                    if (tile.units > 0) {
                        opp_units.emplace_back(tile);
                    } else if (tile.recycler) {
                        opp_recyclers.emplace_back(tile);
                    }
                } else if (tile.scrap_amount > 0) {
                    neutral_tiles.emplace(x * width + y, tile);
                }
            }
        }

        vector<string> actions;

        auto tiles_togo_count = [&tiles, &width, &height](Tile tile){
            vector<pair<int, int>> next_coords;
            next_coords.emplace_back(tile.x - 1, tile.y);
            next_coords.emplace_back(tile.x + 1, tile.y);
            next_coords.emplace_back(tile.x, tile.y - 1);
            next_coords.emplace_back(tile.x, tile.y + 1);

            int count = 0;
            for (auto coord : next_coords) {
                if (coord.first >= width || coord.first < 0 || coord.second >= height || coord.second < 0) continue;

                Tile t = tiles.at(coord.first).at(coord.second);

                if (!t.grass &&
                    ((t.owner == OPP && !t.recycler)
                    || t.owner == NONE)) {
                    ++count;
                }
            }
            return count;
        };

        auto tiles_scrap_count = [&tiles, &width, &height](Tile tile){
            vector<pair<int, int>> next_coords;
            next_coords.emplace_back(tile.x, tile.y);
            next_coords.emplace_back(tile.x - 1, tile.y);
            next_coords.emplace_back(tile.x + 1, tile.y);
            next_coords.emplace_back(tile.x, tile.y - 1);
            next_coords.emplace_back(tile.x, tile.y + 1);

            int scrap = 0;
            for (auto coord : next_coords) {
                if (coord.first >= width || coord.first < 0 || coord.second >= height || coord.second < 0) continue;

                Tile t = tiles.at(coord.first).at(coord.second);

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

        auto tiles_enemy_count = [&tiles, &width, &height](Tile tile){
            vector<pair<int, int>> next_coords;
            next_coords.emplace_back(tile.x - 1, tile.y);
            next_coords.emplace_back(tile.x + 1, tile.y);
            next_coords.emplace_back(tile.x, tile.y - 1);
            next_coords.emplace_back(tile.x, tile.y + 1);

            int enemies = 0;
            for (auto coord : next_coords) {
                if (coord.first >= width || coord.first < 0 || coord.second >= height || coord.second < 0) continue;

                Tile t = tiles.at(coord.first).at(coord.second);

                if (t.owner == OPP && t.units > 0)
                    enemies += t.units;
            }
            return enemies;
        };

        multimap<int, Tile> to_spawn;
        multimap<int, Tile> to_build;
        for (auto tile : my_tiles) {
            if (tile.can_spawn)
                to_spawn.emplace(tiles_togo_count(tile), tile);

            if (tile.can_build)
                to_build.emplace(tiles_enemy_count(tile), tile);
        }

        int size = width * height;
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
            int dist_enemy = max_distance;
            Tile closest_enemy;
            for (const auto&[index, t] : opp_tiles) {
                to_move.emplace(distance(t, tile), make_pair(number(tile, width), number(t, width)));
            }

            int dist_neutral = max_distance;
            Tile closest_neutral;
            for (const auto&[index, t] : neutral_tiles) {
                to_move.emplace(distance(t, tile), make_pair(number(tile, width), number(t, width)));
            }
        }

        unordered_set<int> used_tiles;
        for (auto it = to_move.begin(); it != to_move.end(); it++) {

            int x_s = it->second.first / width;
            int y_s = it->second.first % width;

            int x_t = it->second.second / width;
            int y_t = it->second.second % width;
            Tile & source = tiles.at(x_s).at(y_s);
            Tile & target = tiles.at(x_t).at(y_t);

            if (used_tiles.find(it->second.second) == used_tiles.end() && source.units > 0) {
                int amount = 1; //source.units;
                source.units -= amount;
                ostringstream action;
                action << "MOVE " << amount << " " << source << " " << target;
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
