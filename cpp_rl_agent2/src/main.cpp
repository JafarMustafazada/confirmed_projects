#include "jdevtools/jdevcurl.hpp"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <thread>

using json = nlohmann::json;
using namespace jdevtools;
using namespace std;

#define COL_TURN			"\033[35;1m"
#define COL_CURR			"\033[34;1m"
#define COL_RESET			"\033[0m"


static constexpr int A = 4; // N, E, S, W
static constexpr int GRID_SIZE = 40;
static constexpr int S = GRID_SIZE * GRID_SIZE;

inline static int TIME_DELAY = 6;
inline static int VISUAL_MODE = 0;

class GridAPI {
public:
	inline static int userid1 = 3671;
	inline static int teamid1 = 1447;
	inline static int worldid1 = 1;
	inline static vector<string> haeders;

	static void readyH() {
		string apikey = "";
		{
			ifstream file("apikey.txt");
			if (!file) {
				cout << "\nno apikey.\n";
				return;
			}
			getline(file, apikey);
		}
		haeders = {
			"Content-Type: application/x-www-form-urlencoded",
			"x-api-key: " + apikey,
			"userId: " + to_string(userid1)
		};
	}

	static pair<pair<int, int>, double> makeMove(char direction) {
		requestData req;
		req.headers = haeders;
		req.url = "https://www.notexponential.com/aip2pgaming/api/rl/gw.php";
		req.postData = "type=move&teamId=" + to_string(teamid1) + "&worldId=" + to_string(worldid1) + "&move=" + direction;

		string str = sender(req, (req.postData.size()));
		cout << str;
		json js = json::parse(str);

		if (!js.contains("reward")) {
			cout << "\nno reward\n";
			return {{-1, -1}, 0.0};
		}

		double reward = js["reward"];
		int r = -1, c = -1;

		try {
			if (js["newState"]["x"].is_number())
				r = js["newState"]["x"];
			else
				r = stoi(js["newState"]["x"].get<string>());
	
			if (js["newState"]["y"].is_number())
				c = js["newState"]["y"];
			else
				c = stoi(js["newState"]["y"].get<string>());
		}
		catch(const std::exception& e) {
			r = -1, c = -1;
		}

		return {{r, c}, reward};
	}

	static pair<int, int> getInitialPosition() {
		requestData req;
		req.headers = haeders;
		req.url = "https://www.notexponential.com/aip2pgaming/api/rl/gw.php?type=location&teamId=" + to_string(teamid1);

		string str = sender(req, (req.postData.size()));
		cout << str << '\n';
		json js = json::parse(str);

		int world = -1, r = -1, c = -1;

		if (js["world"].is_number())
			world = js["world"];
		else
			world = stoi(js["world"].get<string>());

		if (world != -1) {
			requestData en;
			en.headers = haeders;
			en.url = "https://www.notexponential.com/aip2pgaming/api/rl/gw.php";
			en.postData = "type=enter&worldId=" + to_string(worldid1) + "&teamId=" + to_string(teamid1);
			string str = sender(req, (req.postData.size()));
			cout << str << '\n';
		}

		if (world != worldid1) {
			cout << "\n error. current is " << world << " while iteration is " << worldid1 << '\n';
			return {-1, -1};
		}

		str = js["state"].get<string>();
		size_t p1 = str.find(':');
		r = stoi(str.substr(0, p1));
		c = stoi(str.substr(p1 + 1));

		return {r, c};
	}
};

struct Cell {
	// For each direction (N,E,S,W), store the resulting position
	pair<int, int> transitions[A];
	int explored[A] = { 0 }; // Whether we've tried this direction
	double rewards[A] = { 0 };        // Rewards received in each direction

};

void to_json(nlohmann::json& j, const Cell& c) {
	auto trans_array = json::array();
	auto explored_array = json::array();
	auto rewards_array = json::array();
	
	for (int i = 0; i < 4; ++i) {
		json trans_obj;
		trans_obj["x"] = c.transitions[i].first;
		trans_obj["y"] = c.transitions[i].second;
		trans_array.push_back(trans_obj);
		explored_array.push_back(c.explored[i]);
		rewards_array.push_back(c.rewards[i]);
	}

	j["transitions"] = trans_array;
	j["explored"] = explored_array;
	j["rewards"] = rewards_array;
}

void from_json(const nlohmann::json& j, Cell& c) {
	auto trans_array = j["transitions"];
	auto explored_array = j["explored"];
	auto rewards_array = j["rewards"];
	
	for (int i = 0; i < 4; ++i) {
		c.transitions[i].first = trans_array[i]["x"];
		c.transitions[i].second = trans_array[i]["y"];
		c.explored[i] = explored_array[i];
		c.rewards[i] = rewards_array[i];
	}
}


inline int idx(int r, int c, int N) { return r * N + c; }
inline pair<int, int> coords(int s, int N) { return {s / N, s % N}; }


class GridExplorer {
	static constexpr int N = 0, E = 1, S = 2, W = 3;
	static constexpr int stuck_point = 4;
	const vector<char> DIRECTIONS = {'N', 'E', 'S', 'W'};
	const vector<char> DIRECTIONS2 = {'v', '>', '^', '<'};
	const vector<pair<int, int> > DIR_VECTORS = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}}; // N, E, S, W

	pair<int, int> currentPos;
	vector<vector<Cell> > world;
	unordered_set<string> knownCells;
	pair<int, int> targetPos = {-1, -1};
	char targetMove = '-';
	bool targetFound = false;
	random_device rd;
	mt19937 rng;

	void save() {
		ofstream file("world_" + to_string(GridAPI::worldid1) + "_mapv2.json");
		json saveData;
		saveData["world"] = world;
		saveData["knownCells"] = knownCells;
		saveData["targetFound"] = targetFound;
		saveData["targetPos"] = targetPos;
		saveData["targetMove"] = targetMove;
		file << saveData.dump(2);
	}

	void load() {
		{
			ifstream file("world_" + to_string(GridAPI::worldid1) + "_mapv2.json");
			if (!file) return;
			json js;
			file >> js;
			world = js["world"].get<vector<vector<Cell> > >();
			knownCells = js["knownCells"].get<unordered_set<string> >();
			targetFound = js["targetFound"].get<bool>();
			targetPos = js["targetPos"].get<pair<int, int> >();
			targetMove = js["targetMove"].get<char>();
		}

		{
			ifstream file("world_" + to_string(GridAPI::worldid1) + "_tabv2.json");
			if (file) {
				json js;
				file >> js;
				unordered_map<string, vector<double> > tab = js["Q"].get<unordered_map<string, vector<double> > >();
				for (auto &cell: tab) {
					if (!knownCells.count(cell.first)) knownCells.insert(cell.first);
				}
			}
		}

		{
			ifstream file("world_" + to_string(GridAPI::worldid1) + "_tab.json");
			if (file) {
				json js;
				file >> js;
				vector<vector<double> > tab = js["Q"].get<vector<vector<double> > >();
				for (int i = 0; i < S; i++) {
					bool addit = false;
					for (int k = 0; k < A; k++) {
						if (tab[i][k] != 1.0) {
							addit = true;
							break;
						}
					}
					if (addit) {
						string key = posToString(coords(i, GRID_SIZE));
						if (!knownCells.count(key)) knownCells.insert(key);
					}
				}
			}
		}
	}

	// Convert position to string key for sets/maps
	string posToString(const pair<int, int> &pos) {
		return to_string(pos.first) + ":" + to_string(pos.second);
	}

	// Checks if coordinates are valid (within grid)
	bool isValid(int x, int y) {
		return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE;
	}

	// Direction index to char
	char directionChar(int dir) {
		return DIRECTIONS[dir];
	}

	// Direction char to index
	int directionIndex(char dir) {
		switch (dir) {
		case 'N':
			return N;
		case 'E':
			return E;
		case 'S':
			return S;
		case 'W':
			return W;
		default:
			return -1;
		}
	}

	// Dijkstra's algorithm for pathfinding
	vector<char> findPath(const pair<int, int> &start, const pair<int, int> &goal) {
		struct Node {
			pair<int, int> pos;
			double cost;
			vector<char> path;

			bool operator>(const Node &other) const {
				return cost > other.cost;
			}
		};

		priority_queue<Node, vector<Node>, greater<Node>> pq;
		pq.push({{start.first, start.second}, 0, {}});

		unordered_map<string, double> costSoFar;
		costSoFar[posToString(start)] = 0;

		while (!pq.empty()) {
			Node current = pq.top();
			pq.pop();

			if (current.pos == goal) {
				return current.path;
			}

			for (int dir = 0; dir < 4; dir++) {
				// Skip if we haven't explored this direction yet
				if (!world[current.pos.first][current.pos.second].explored[dir]) {
					continue;
				}

				pair<int, int> nextPos = world[current.pos.first][current.pos.second].transitions[dir];
				if (!isValid(nextPos.first, nextPos.second)) continue;
				if (current.pos.first == nextPos.first && current.pos.second == nextPos.second) continue;
				

				// Cost is 1 for each move (could be adjusted based on rewards)
				double newCost = current.cost + 1;
				string nextPosStr = posToString(nextPos);

				if (!costSoFar.count(nextPosStr) || newCost < costSoFar[nextPosStr]) {
					costSoFar[nextPosStr] = newCost;

					// Create new path by adding this direction
					vector<char> newPath = current.path;
					newPath.push_back(directionChar(dir));

					pq.push({nextPos, newCost, newPath});
				}
			}
		}

		// No path found
		return {};
	}

	// returns {x,y} of nearest undiscovered cell, or {-1,-1} if all reachable are known
	pair<int,int> findNearestUnvisitedCell(int startX, int startY) {
		queue<pair<int,int> > q;
		unordered_set<string> visited;
		
		auto pushIfUnseen = [&](int x, int y) {
			string key = posToString({x,y});
			if (!visited.count(key) && isValid(x,y)) {
				visited.insert(key);
				q.push({x,y});
			}
		};
		
		// start BFS from your current position
		visited.insert(posToString({startX, startY}));
		q.push({startX, startY});
		
		while (!q.empty()) {
			auto [x,y] = q.front(); 
			q.pop();
			
			// if this cell hasn’t been discovered yet, we’re done
			if (!knownCells.count(posToString({x,y}))) {
				return {x,y};
			}
			
			// otherwise enqueue its 4 neighbors
			static const vector<pair<int,int> > dirs = {
				{1,0}, {-1,0}, {0,1}, {0,-1}
			};
			for (auto [dx,dy] : dirs) {
				pushIfUnseen(x + dx, y + dy);
			}
		}
		
		// no unknown cell reachable
		return {-1,-1};
	}

	int visit_count(Cell &cell) {
		int visited = 0;
		for (int dir = 0; dir < 4; dir++) {
			if (cell.explored[dir]) visited++;
		}
		return visited;
	}
	
	// Choose which direction to move for exploration
	int chooseExplorationMove(pair<int, int> nearestUnvisited = {-1, -1}) {
		// 1 priority: Move toward unvisited cells
		if (nearestUnvisited.first == -1) nearestUnvisited = findNearestUnvisitedCell(currentPos.first, currentPos.second);
		if (nearestUnvisited.first != -1) {
			// vector<char> path = findPath(currentPos, nearestUnvisited);
			// if (!path.empty()) {
			// 	return directionIndex(path[0]);
			// }
			// if no know path to cell, follow the wind
			int dx = currentPos.first - nearestUnvisited.first;
			int dy = currentPos.second - nearestUnvisited.second;
			if (abs(dx) >= abs(dy)) {
				if (dx > 0) return W;
				else return E;
			}
			else {
				if (dy > 0) return S;
				else return N;
			}
		}
		cout << "\n\n!!all discovered??\n\n";

		// 2 priority: Unexplored moves from current position
		// vector<int> unexploredMoves;
		// for (int dir = 0; dir < 4; ++dir) {
		// 	if (!world[currentPos.first][currentPos.second].explored[dir]) {
		// 		unexploredMoves.push_back(dir);
		// 	}
		// }
		
		// if (!unexploredMoves.empty()) {
		// 	// Choose random unexplored direction
		// 	uniform_int_distribution<int> dist(0, unexploredMoves.size() - 1);
		// 	return unexploredMoves[dist(rng)];
		// }
		
		// // 3 priority: Move to least visited area
		// pair<int, int> leastVisited = findLeastVisitedNeighbor();
		// if (leastVisited.first != -1) {
		//     // Find direction that most likely leads to least visited
		//     for (int dir = 0; dir < 4; ++dir) {
		//         if (world[currentPos.first][currentPos.second].exploredMove[dir]) {
		//             int mostLikelyDir = getMostLikelyResultingDirection(currentPos, dir);
		//             if (mostLikelyDir >= 0) {
		//                 pair<int, int> resultPos = world[currentPos.first][currentPos.second].transitions[dir];
		//                 if (resultPos == leastVisited) {
		//                     return dir;
		//                 }
		//             }
		//         }
		//     }
		// }
		
		// Fallback: Random direction
		uniform_int_distribution<int> dist(0, 3);
		return dist(rng);
	}

	// Determine which direction actually happened based on position change
	int determineActualDirection(const pair<int, int>& from, const pair<int, int>& to) {
		if (to.first == -1 || to.second == -1) return -2;
		int dx = to.first - from.first;
		int dy = to.second - from.second;
		
		// Normalize to single step
		if (dx > 0) dx = 1;
		else if (dx < 0) dx = -1;
		if (dy > 0) dy = 1;
		else if (dy < 0) dy = -1;
		
		// Handle no movement case (wall/obstacle)
		if (dx == 0 && dy == 0) {
			return -1; // No movement
		}
		
		// Match with direction vectors
		for (int dir = 0; dir < 4; ++dir) {
			if (dx == DIR_VECTORS[dir].first && dy == DIR_VECTORS[dir].second) {
				return dir;
			}
		}
		
		// Diagonal movement or multi-step movement (shouldn't happen in 4-directional grid)
		cout << "Warning: Unexpected movement detected!" << endl;
		return -2;
	}

	// Explore the grid to find targets
	void explore() {
		const int MAX_STEPS = 5000;
		int steps = 0;
		int stuckCounter = 0;
		knownCells.insert(posToString(currentPos));

		while (!targetFound && steps < MAX_STEPS) {
			auto wait_until = chrono::system_clock::now() + chrono::seconds(TIME_DELAY);
			steps++;

			// Choose which direction to move
			int moveDir = chooseExplorationMove();

			// if stuck, choose least explored direction
			if (stuckCounter >= stuck_point || world[currentPos.first][currentPos.second].explored[moveDir] >= stuck_point) {
				int index = -1;
				int minVisits = numeric_limits<int>::max();
				int minVisits2 = numeric_limits<int>::max();
				Cell &current = world[currentPos.first][currentPos.second];
				for (int i = 0; i < A; i++) {
					if (!isValid(currentPos.first + DIR_VECTORS[i].first, 
						currentPos.second + DIR_VECTORS[i].second)) continue;
					if (minVisits > current.explored[i] + 3) {
						minVisits = current.explored[i];
						index = i;
					}
				}
				moveDir = index;
			}

			// Make the move
			auto [newPos, reward] = GridAPI::makeMove(directionChar(moveDir));
			int chosenDir = determineActualDirection(currentPos, newPos);
			cout << " " << DIRECTIONS2[moveDir] << " " << directionChar(moveDir);
			if (chosenDir > -1) cout << " " << DIRECTIONS2[chosenDir] << '\n';
			else cout << " | \n";

			// Update our knowledge
			if (reward >= 1000) {
				targetFound = true;
				targetPos = currentPos;
				targetMove = directionChar(moveDir);
				cout << "Target found at: " << currentPos.first << "," << currentPos.second
					<< " with reward: " << reward << endl;
				break;
			}
			else if (currentPos == newPos) {
				stuckCounter++;
				chosenDir = moveDir;
				world[currentPos.first][currentPos.second].explored[chosenDir]++;
				if (stuckCounter >= stuck_point) {
					// it is most certanly wall, and our head needs a bit healing from hitting it.
					world[currentPos.first][currentPos.second].transitions[chosenDir] = newPos;
					world[currentPos.first][currentPos.second].rewards[chosenDir] = reward;
				}
			}
			else{
				stuckCounter = 0;
				world[currentPos.first][currentPos.second].explored[chosenDir] = true;
				world[currentPos.first][currentPos.second].transitions[chosenDir] = newPos;
				world[currentPos.first][currentPos.second].rewards[chosenDir] = reward;
			}

			// Update current position
			currentPos = newPos;
			knownCells.insert(posToString(currentPos));

			save();
			if (VISUAL_MODE) visualizeGrid();

			// Print status occasionally
			if (steps % 100 == 0) {
				cout << "\nExploration step " << steps << ", visited "
					<< knownCells.size() << " cells." << endl;
			}
			
			cout << "asleep..";
			this_thread::sleep_until(wait_until);
			cout << "awake.. ";
		}

		cout << "Exploration complete after " << steps << " steps." << endl;
	}

	// Using learned information to find the optimal path to the target
	void findOptimalPath() {
		if (!targetFound) {
			cout << "No target found yet." << endl;
			return;
		}

		cout << "Finding optimal path to target at "
			<< targetPos.first << "," << targetPos.second << endl;

		// Get and follow the optimal path using Dijkstra
		vector<char> optimalPath = findPath(currentPos, targetPos);

		if (optimalPath.empty()) {
			cout << "Cannot find path to target!" << endl;
			return;
		}

		cout << "Optimal path length: " << optimalPath.size() << endl;

		// Follow the path and measure performance
		double totalReward = 0;
		for (char direction : optimalPath) {
			auto wait_until = chrono::system_clock::now() + chrono::seconds(TIME_DELAY);
			auto [newPos, reward] = GridAPI::makeMove(direction);
			currentPos = newPos;
			totalReward += reward;

			if (reward >= 1000) {
				cout << "Target reached! Total reward: " << totalReward << endl;
				return;
			}


			cout << "\nasleep..";
			this_thread::sleep_until(wait_until);
			cout << "awake.. ";
		}

		cout << "Path followed but target not reached. Total reward: " << totalReward << endl;
	}

public:
	GridExplorer() : rng(rd()) {
		// Initialize the world grid
		world.resize(GRID_SIZE, vector<Cell>(GRID_SIZE));

		// Initialize expected transitions (before exploration)
		for (int i = 0; i < GRID_SIZE; i++) {
			for (int j = 0; j < GRID_SIZE; j++) {
				for (int dir = 0; dir < 4; dir++) {
					int ni = i + DIR_VECTORS[dir].first;
					int nj = j + DIR_VECTORS[dir].second;
					world[i][j].explored[dir] = 0;

					// Expected result of move (might be changed during exploration)
					if (isValid(ni, nj)) {
						world[i][j].transitions[dir] = {ni, nj};
					} else {
						// Out of bounds - expect to stay in place
						world[i][j].transitions[dir] = {i, j};
					}
				}
			}
		}

		// Get initial position
		currentPos = GridAPI::getInitialPosition();
		load();
	}

	void run(bool optimal = false) {
		cout << "Starting grid exploration..." << endl;

		if (optimal) {
			// Second phase: find optimal path to target
			findOptimalPath();
		} else {
			// First phase: explore and build the map
			explore();
			if (!targetFound) cout << "No target found during exploration." << endl;
		}

		// Report results
		printStats();
	}

	void printStats() {
		int exploredCells = knownCells.size();
		int totalDirections = exploredCells * 4;
		int exploredDirections = 0;

		for (const auto &posStr : knownCells) {
			int commaPos = posStr.find(',');
			int i = stoi(posStr.substr(0, commaPos));
			int j = stoi(posStr.substr(commaPos + 1));

			for (int dir = 0; dir < 4; ++dir) {
				if (world[i][j].explored[dir]) {
					exploredDirections++;
				}
			}
		}

		cout << "Map statistics:" << endl;
		cout << "- Visited cells: " << exploredCells << " of " << GRID_SIZE * GRID_SIZE << endl;
		cout << "- Explored directions: " << exploredDirections << " of " << totalDirections << endl;

		if (targetFound) {
			cout << "- Target found at: " << targetPos.first << "," << targetPos.second << endl;
		} else {
			cout << "- No target found" << endl;
		}
	}
	
	void getToTarget() {
		if (!targetFound) {
			cout << "No target found yet." << endl;
			return;
		}

		int stuckCounter = 0;
		knownCells.insert(posToString(currentPos));

		while (currentPos != targetPos) {
			auto wait_until = chrono::system_clock::now() + chrono::seconds(TIME_DELAY);

			int moveDir = chooseExplorationMove(targetPos);
			auto [newPos, reward] = GridAPI::makeMove(directionChar(moveDir));

			int chosenDir = determineActualDirection(currentPos, newPos);
			cout << " " << DIRECTIONS2[moveDir] << " " << directionChar(moveDir);
			if (chosenDir > -1) cout << " " << DIRECTIONS2[chosenDir] << '\n';
			else cout << " | \n";

			// Update our knowledge
			if (reward >= 1000) {
				cout << "Target found at: " << currentPos.first << "," << currentPos.second
					<< " with reward: " << reward << endl;
				return;
			}

			currentPos = newPos;
			if (VISUAL_MODE) visualizeGrid();
			
			cout << "asleep..";
			this_thread::sleep_until(wait_until);
			cout << "awake.. ";
		}

		GridAPI::makeMove(targetMove);
	}

	void visualizeGrid(int radius = 40) {
		int cx = currentPos.first;
		int cy = currentPos.second;
	
		int minX = max(0, cx - radius);
		int maxX = min(GRID_SIZE - 1, cx + radius);
		int minY = max(0, cy - radius);
		int maxY = min(GRID_SIZE - 1, cy + radius);
	
		cout << "Grid visualization (around current position):" << endl;
	
		cout << "   ";
		for (int x = minX; x <= maxX; ++x) {
			cout << (x % 10) << ' ';
		}
		cout << '\n';
	
		for (int y = minY; y <= maxY; ++y) {
			// row header
			cout << (y % 10) << ": ";
	
			for (int x = minX; x <= maxX; ++x) {
				string posStr = posToString({x, y});
	
				if (x == cx && y == cy) {
					cout << COL_CURR << "C" << COL_RESET << " ";
				} else if (x == targetPos.first && y == targetPos.second && targetFound) {
					cout << "T ";
				} else if (knownCells.count(posStr)) {
					cout << visit_count(world[x][y]) << ' ';
				} else {
					cout << COL_TURN << "?" << COL_RESET << " ";
				}
			}
			cout << '\n';
		}
	}
};

int main(int argc, char **argv) {
	int world1 = 3, userid1 = 3671, teamid1 = 1447, timedelay = 10, visual = 0;
	string argument = (argc > 1) ? (argv[1]) : ("-help");

	cout << "total arguments: " << int((argc - 1) / 2) << "\n";
	if (argument == "-help") {
		cout << "\nneed at least one argument to start like: -userid\n";
		cout << "-userid {default(3671)}\n";
		cout << "-teamid {default(1447)}\n";
		cout << "-world {which world we learning. default(3)}\n";
		cout << "-time {time delay in seconds between moves. default(10)}\n";
		cout << "-visual {1 - show map every move. 2 - show map and end program. default(0)}\n";
		return 0;
	}

	argc--;
	for (int i = 1; i < argc; i += 2) {
		string argument = argv[i];
		cout << argument << " " << argv[i + 1] << "\n";
		if (false)
			;
		else if (argument == "-userid")
			userid1 = stoi(argv[i + 1]);
		else if (argument == "-teamid")
			teamid1 = stoi(argv[i + 1]);
		else if (argument == "-world")
			world1 = stoi(argv[i + 1]);
		else if (argument == "-time")
			timedelay = stoi(argv[i + 1]);
		else if (argument == "-visual")
			visual = stoi(argv[i + 1]);
		else {
			cout << "Error with param:{" << argument << "}\n";
			return -1;
		}
	}

	GridAPI::teamid1 = teamid1;
	GridAPI::userid1 = userid1;
	GridAPI::worldid1 = world1;
	GridAPI::readyH();

	TIME_DELAY = timedelay;
	VISUAL_MODE = visual;

	GridExplorer explorer;
	explorer.printStats();
	explorer.visualizeGrid();
	
	if (visual != 2) explorer.run();

	cout << "\nProgram complete." << endl;
	return 0;
}
