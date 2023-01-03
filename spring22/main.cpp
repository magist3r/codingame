#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;

enum EntityType { Monster = 0, Hero = 1, EnemyHero = 2 };
enum ThreatType { No = 0, My = 1, Enemy = 2 };
enum CommandType { Wait = 0, Move, Wind, Control, Shield };

struct Entity {
  int id = 0;                // Unique identifier
  EntityType type = Monster; // 0=monster, 1=your hero, 2=opponent hero
  int x = 0;                 // Position of this entity
  int y = 0;
  int shieldLife = 0; // Ignore for this league; Count down until shield spell fades
  bool isControlled = false; // Ignore for this league; Equals 1 when this
                             // entity is under a control spell
  int health = 0;            // Remaining health of this monster
  int vx = 0;                // Trajectory of this monster
  int vy = 0;
  bool nearBase = false; // 0=monster with no target yet, 1=monster targeting a base
  ThreatType threatFor = No; // Given this monster's trajectory, is it a threat to 1=your base,
          // 2=your opponent's base, 0=neither
  bool hidden = false;
};

struct Command {
  Command() {}
  Command(CommandType type, int x, int y, int id = -1)
      : m_type(type), m_x(x), m_y(y), m_id(id) {}
  string toString() const {
    switch (m_type) {
    case Wait:
      return "WAIT";
    case Move:
      return "MOVE " + to_string(m_x) + " " + to_string(m_y);
    case Wind:
      return "SPELL WIND " + to_string(m_x) + " " + to_string(m_y);
    case Control:
      return "SPELL CONTROL " + to_string(m_id) + " " + to_string(m_x) + " " +
             to_string(m_y);
    case Shield:
      return "SPELL SHIELD " + to_string(m_id);
    }
  }

private:
  CommandType m_type = Wait;
  int m_x = 0;
  int m_y = 0;
  int m_id = -1; // For CONTROL
};

constexpr int minDistance = 2500;
//constexpr int controlDistance = 5000;
constexpr int maxDistance = 9000;
constexpr int patrolDistance = 7000;
constexpr float PI = 3.14159265 / 180;
constexpr int windRadius = 1280;
constexpr int controlRadius = 2200;
constexpr int dangerDistance = 6000;
constexpr int dangerRadius = windRadius;
constexpr int dangerAcceleration = 400;

static int baseX;
static int baseY;
static int enemyBaseX;
static int enemyBaseY;
static bool leftSide;
static int manaPool;

float calcDistance(int x1, int y1, int x2, int y2) {
  return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

pair<int, int> calcControlTarget(Entity target) {
  if (target.nearBase) {
    return {enemyBaseX, enemyBaseY};
  } else {
    if (leftSide) {
      return { target.x * 3, target.y * 3};
      if (target.x > 5000 && target.y < 4500)
        return {target.x, enemyBaseY};
      else if (target.x > 5000 && target.y >= 4500)
        return {target.x, baseY};
      else if (target.y > 5000)
        return {baseX, target.y};
      else
        return {enemyBaseX, enemyBaseY};
    } else {
        return { target.x  / 3, target.y / 3};
      if (target.x < baseX - 5000 && target.y < 4500)
        return {target.x, baseY};
      if (target.x < baseX - 5000 && target.y >= 4500)
        return {target.x, enemyBaseY};
      else if (target.y < baseY - 5000)
        return {baseX, target.y};
      else
        return {enemyBaseX, enemyBaseY};
    }
  }
}

// return closest id and distance
pair<int, float> findClosest(const Entity &entity,
                             const map<int /*id*/, Entity> &targets,
                             bool sector = false) {
  float minDist{20000}, id{-1};
  for (const auto &target : targets) {
    int dist =
        calcDistance(entity.x, entity.y, target.second.x, target.second.y);
    if (dist < minDist) {
      if (sector) {
        int angle{0};
        if (leftSide) {
          angle = atan2(target.second.y, target.second.x) * 180. / 3.1415;
        } else {
          angle = atan2((baseY - target.second.y), (baseX - target.second.x)) *
                  180. / 3.1415;
        }
        if (entity.id != angle / 30)
          continue;
      }
      minDist = dist;
      id = target.second.id;
    }
  }
  return {id, minDist};
}

map<int /*distance*/, Entity>
monstersHeadingSpy(const Entity &spy, const map<int /*id*/, Entity> &monsters, map<int, int> & distanceToEnemy) {
  map<int, Entity> result;
  for (const auto &monster : monsters) {
    auto it = distanceToEnemy.find(monster.first);
    int dist1 = it != distanceToEnemy.end() ? distanceToEnemy[monster.first] : 0;
    cerr << dist1 << endl;
    int dist2 = calcDistance(spy.x, spy.y, monster.second.x, monster.second.y);
    
    //int dist2 = calcDistance(spy.x, spy.y,
    //                         monster.second.x + monster.second.vx,
    //                         monster.second.y + monster.second.vy);
    int accel = dist2 - dist1;

    distanceToEnemy[monster.first] = dist2;

    if ((accel < 0 && dist2 + accel*2 <= dangerRadius) || dist2 < controlRadius) {
      result.emplace(dist2 + accel*2, monster.second); // store worst (smallest) result
      cerr << "monsterID: " << monster.second.id << ", danger: " << dist2 << " " << accel << endl;
    }
  }
  return result;
}

bool canCastSpell(CommandType spell, int mana, const Entity &hero, const Entity &target, const map<int /*id*/, Entity> &monsters) {
    switch (spell) {
        case Wait:
        case Move:
            return false;
        case Wind:
        {
            if (mana < 10 || calcDistance(hero.x, hero.y, target.x, target.y) > windRadius) return false;

            for (const auto & monster : monsters) {
                if (calcDistance(hero.x, hero.y, monster.second.x, monster.second.y) <= windRadius && monster.second.shieldLife == 0)
                    return true;
            }

            return false;
        }
        case Control:
            return true;
        case Shield:
            return true;
    }
}

pair <int, int> getPerpendicularVector(int vx, int vy, int directionX = 1, int distance = 400) {
    float y = -vx * directionX * 1. / vy;
    return { directionX * distance, y * distance };
}

void updateHiddenMonsters(const map<int, Entity>& monsters, map<int, Entity>& allMonsters, const map<int, Entity>& heroes) {
    // Remove visible
    for (const auto &monster : monsters) {
        auto it = allMonsters.find(monster.first);
        if (it != allMonsters.end())
            allMonsters.erase(it);
    }

    // Update still hidden
    for (auto it = allMonsters.begin(); it != allMonsters.end(); ) { 
        it->second.x += it->second.vx;
        it->second.y += it->second.vy;
        int distance = calcDistance(baseX, baseY, it->second.x, it->second.y);
        if (it->second.x < 0 || it->second.x > 17630 ||
        it->second.y < 0 || it->second.y > 9000 || distance > 9000 || it->second.health <= 2 || it->second.isControlled) {
            it = allMonsters.erase(it);
        } else {
            it->second.hidden = true;
            it++;
        }
    }

    // remove visible monsters
    for (auto it = allMonsters.begin(); it != allMonsters.end(); ) {
        if (it->second.hidden) {
            if (calcDistance(baseX, baseY, it->second.x, it->second.y) <= 6000) // Visible by base
                it = allMonsters.erase(it);
            else if (findClosest(it->second, heroes).second <= 2200) // Visible by hero
                it = allMonsters.erase(it);
            else
                ++it;
        }
        else
            ++it;
        
    } 

    // Store all visible again
    for (const auto &monster : monsters) {
        allMonsters.emplace(monster.first, monster.second);
    }
}

int main() {
  int base_x; // The corner of the map representing your base
  int base_y;
  cin >> base_x >> base_y;
  cin.ignore();
  int heroes_per_player; // Always 3
  cin >> heroes_per_player;
  cin.ignore();

  leftSide = base_x == 0 ? true : false;

  enemyBaseX = leftSide ? 17630 : 0;
  enemyBaseY = leftSide ? 9000 : 0;

  baseX = base_x;
  baseY = base_y;

  map<int /*id*/, Entity> allMonsters;
  map<int, int> distanceToEnemy;

  // game loop
  while (1) {
    int health; // Each player's base health
    vector<int> mana(
        2); // Ignore in the first league; Spend ten mana to cast a spell
    for (int i = 0; i < 2; i++) {
      cin >> health >> mana[i];
      cin.ignore();
    }
    int entity_count; // Amount of heros and monsters you can see
    cin >> entity_count;
    cin.ignore();

    unordered_map<int /*id*/, Entity> entities;
    map<int /*id*/, Entity> monsters2;
    map<int /*id*/, Entity> myHeroes;
    map<int /*id*/, Entity> enemyHeroes;

    multimap<int /*distance*/, int /*id*/> targets;
    map<int /*hero*/, pair<int, int>> moves;
    map<int /*hero*/, Command> commands;

    map<int /*id*/, Entity> dangerMonsters;

    bool hasThreats {false};

    for (int i = 0; i < entity_count; i++) {
      Entity entity;

      int type;
      int is_controlled;
      int near_base;
      int threat_for;
      cin >> entity.id >> type >> entity.x >> entity.y >> entity.shieldLife >>
          is_controlled >> entity.health >> entity.vx >> entity.vy >>
          near_base >> threat_for;
      cin.ignore();

      entity.type = static_cast<EntityType>(type);
      entity.threatFor = static_cast<ThreatType>(threat_for);
      entity.isControlled = static_cast<bool>(is_controlled);
      entity.nearBase = static_cast<bool>(near_base);

      entities[entity.id] = entity;
      int distance = calcDistance(entity.x + entity.vx, entity.y + entity.vy, base_x, base_y);
      switch (entity.type) {
      case Monster: {
        if (distance <= maxDistance) { // skip targets > 9000 from the base
          monsters2[entity.id] = entity;
        }
        if (entity.threatFor == ThreatType::My) {
          if (distance <= maxDistance) { // skip targets > 9000 from the base
            targets.emplace(distance, entity.id);
            if (entity.nearBase)
                hasThreats = true;
          }
        }

        break;
      }
      case Hero: {
        entity.id = leftSide ? entity.id : entity.id - heroes_per_player;
        myHeroes[entity.id] = entity;
        break;
      }
      case EnemyHero:
        enemyHeroes[leftSide ? entity.id - heroes_per_player : entity.id] =
            entity;
        break;
      }
    }

    updateHiddenMonsters(monsters2, allMonsters, myHeroes);
    for (const auto & hidden : allMonsters) {
        cerr << hidden.first << ": " << hidden.second.hidden << " " << hidden.second.x << " " << hidden.second.y << " " << hidden.second.health << endl;
    }
    
    //monsters = allMonsters;

    auto _defInsideBase = [&](int id) {
      Entity monster = allMonsters[id];
      auto res = findClosest(monster, myHeroes);
      if (res.second <= windRadius && mana[0] >= 10) {
        commands[res.first] = Command(Wind, enemyBaseX, enemyBaseY);
        mana[0] -= 10;
      } else { // Move closer to the monster
        commands[res.first] = Command(Move, monster.x, monster.y);
      }

      myHeroes.erase(res.first);
      auto it = myHeroes.cbegin();
      while (it != myHeroes.cend()) {
        commands[it->first] = Command(Move, monster.x, monster.y);
        it = myHeroes.erase(it);
      }
    };

    bool isDefInsideBase{true};
    for (const auto &hero : myHeroes) {
      if (calcDistance(hero.second.x, hero.second.y, baseX, baseY) > 5000)
        isDefInsideBase = false;
    }

    if (!targets.empty() && 
        (targets.begin()->first <= minDistance || // First target is too close
        (targets.begin()->first < 5200 &&
         isDefInsideBase))) { // OR def inside base
      _defInsideBase(targets.begin()->second);
    }

    // Oh-ohh
    for (const auto &spy : enemyHeroes) {
      auto dangerMonsters = monstersHeadingSpy(spy.second, allMonsters, distanceToEnemy);

      for (const auto & mon : dangerMonsters) {
      if (!myHeroes.empty()) { // Holy shit
        Entity monster = mon.second;
        auto heroCl = findClosest(monster, myHeroes);
        bool justMove {false};
        cerr << "monsterID: " << monster.id << " heroID: " << heroCl.first << " distance: " << heroCl.second << " mana: " << mana[0] << endl; 
        if (monster.shieldLife == 0 && !monster.hidden && mana[0] >= 10) { // USE canCast method here!
          if (monster.health > 4 || heroCl.second > 800) {
            if (heroCl.second <= windRadius) {
                commands[heroCl.first] = Command(Wind, enemyBaseX, enemyBaseY);
                mana[0] -= 10;
                myHeroes.erase(heroCl.first);
                cerr << "Wind" << endl;
            } else if (heroCl.second <= controlRadius) {
                //auto destVector = getPerpendicularVector(spy.second.vx, spy.second.vy, leftSide ? 1 : -1, 1000);
                auto coords = calcControlTarget(monster);
                commands[heroCl.first] = Command(Control, coords.first, coords.second, monster.id);
                myHeroes.erase(heroCl.first);
                mana[0] -= 10;
            } else if (spy.second.shieldLife == 0) { // try to control the fucking spy
                auto heroCl2 = findClosest(spy.second, myHeroes);
                auto coords = calcControlTarget(spy.second);
                if (heroCl2.second <= controlRadius) {
                    commands[heroCl2.first] = Command(Control, coords.first, coords.second, spy.second.id);
                    myHeroes.erase(heroCl2.first);
                    mana[0] -= 10;
                }
                else
                    justMove = true;
            }
            else
              justMove = true;
          }
          else
            justMove = true;
        }
        else if (monster.health <= 4 && heroCl.second <= 800)
            justMove = true;

        if (justMove) { // Move closer to the monster
            commands[heroCl.first] = Command(Move, monster.x, monster.y);
            myHeroes.erase(heroCl.first);
        }
        
      }
    }
    }

     // cast shield near enemy
    if (mana[0] > 30) {
      auto itH = myHeroes.cbegin();
      while (itH != myHeroes.cend()) {
        if (mana[0] >= 10 &&
            findClosest(itH->second, enemyHeroes).second <=
                controlRadius + 1 &&
            itH->second.shieldLife <= 1) {
          commands[itH->first] =
              Command(Shield, 0, 0, leftSide ? itH->first : itH->first + 3);
          mana[0] -= 10;
          itH = myHeroes.erase(itH);
        } else {
          itH++;
        }
      }
    }

    for (const auto &target : targets) {
      Entity monster = entities[target.second];

      if (!myHeroes.empty()) {
        auto closestHero = findClosest(monster, myHeroes);
        if (monster.nearBase) {
          if (canCastSpell(Wind, mana[0], myHeroes[closestHero.first], monster,
                           allMonsters)) {
            commands[closestHero.first] = Command(Wind, enemyBaseX, enemyBaseY);
            mana[0] -= 10;
          } else {
            commands[closestHero.first] =
                Command(Move, monster.x, monster.y);
          }
        } else {
          if (closestHero.second <= controlRadius && mana[0] >= 10 && !enemyHeroes.empty()) {
            auto coords = calcControlTarget(monster);
            commands[closestHero.first] =
                //Command(Control, enemyBaseX, enemyBaseY, monster.id);
                Command(Control, coords.first, coords.second, monster.id);
            mana[0] -= 10;
          } else {
            commands[closestHero.first] =
                Command(Move, monster.x, monster.y);
          }
        }
        myHeroes.erase(closestHero.first);

        if (monster.nearBase) { // move all other heroes there
          auto it = myHeroes.cbegin();
          while (it != myHeroes.cend()) {
            commands[it->first] = Command(Move, monster.x, monster.y);
            it = myHeroes.erase(it);
          }
          break;
        }
      } else
        break;
    }

    // if we still have available heroes
    auto it = myHeroes.cbegin();
    while (it != myHeroes.cend()) {
      auto res = findClosest(it->second, allMonsters, true);
      if (res.first > 0) {
        commands[it->first] =
            Command(Move, allMonsters[res.first].x, allMonsters[res.first].y);
      } else {
        if (leftSide) {
          commands[it->first] =
              Command(Move, patrolDistance * cos((15 + 30 * it->first) * PI),
                      patrolDistance * sin((15 + 30 * it->first) * PI));
        } else {
          commands[it->first] = Command(
              Move, base_x - patrolDistance * cos((15 + 30 * it->first) * PI),
              base_y - patrolDistance * sin((15 + 30 * it->first) * PI));
        }
      }
      it = myHeroes.erase(it);
    }

    for (int i = 0; i < heroes_per_player; i++) {
      cout << commands[i].toString() << endl;
    }
  }
}