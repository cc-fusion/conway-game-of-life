#include <iostream>
#include <string>
#include <cassert>
#include <memory>
#include <vector>
#include <array>
#include <sstream>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <list>
#include <utility>
#include <unordered_set>

constexpr int treeCacheCapacity {10'000};
constexpr int evolutionCacheCapacity {10'000};

/*
  n
w # e
  s
*/

/* ---------- Forward Declarations ---------- */

class QuadTree;
std::shared_ptr<QuadTree> createEmptyQuadTree (int depth);
std::shared_ptr<QuadTree> createQuadTree(const std::shared_ptr<QuadTree> nw, const std::shared_ptr<QuadTree> ne, const std::shared_ptr<QuadTree> sw, const std::shared_ptr<QuadTree> se);
std::shared_ptr<QuadTree> createQuadTree(bool nw, bool ne, bool sw, bool se);

/* ---------- LRU Cache ---------- */

// https://medium.com/@shahjui2000/the-o-1-solution-mastering-the-lru-cache-with-modern-c-416afc0bfe83
template <typename T_key, typename T_val, typename T_hash = std::hash<T_key>>
class LRU {
private:
    using CacheList = std::list<std::pair<T_key, T_val>>;
    CacheList usedList;
    std::unordered_map<T_key, typename CacheList::iterator, T_hash> lookupTable;
    int capacity;
public:
    LRU(int capacity) {
        this->capacity = capacity;
    }
    void resize(int newCapacity) {
        this->capacity = newCapacity;
        while (lookupTable.size() > newCapacity) {
            // erase the last (used) element of the list
            auto last {usedList.back()};
            lookupTable.erase(last.first); // last.first in this context returns the key
            usedList.pop_back();
        }
    }
    bool contains(T_key key) const {
        return lookupTable.contains(key);
    }
    size_t size() const {
        return lookupTable.size();
    }
    T_val at(T_key key) const {
        return lookupTable.at(key)->second;
    }
    const T_val operator[](T_key key) const {
        return lookupTable.at(key)->second;
    }
    T_val& operator[](T_key key) {
        // Case: key already exists
        if (lookupTable.contains(key)) {
            auto it {lookupTable.at(key)};
            // move value to most recent
            usedList.splice(usedList.begin(), usedList, it);
            return it->second; // return key for writing
        }
        // Case: cache is full, evict last used item
        if (static_cast<int>(lookupTable.size()) >= capacity) {
            // evict last used item
            auto last {usedList.back()};
            lookupTable.erase(last.first); // .first gives the key from the std::pair<T_key, T_val>
            usedList.pop_back();
        }
        /*
        list.front returns a value
        list.begin returns a bidirectional pointer
        */
        usedList.emplace_front(key, T_val{}); // T_val{} creates a default value
        lookupTable[key] = usedList.begin();
        return usedList.front().second;
    }
};

/* ---------- Memoization ---------- */

struct QuadTreeKey {
    QuadTree* nw;
    QuadTree* ne;
    QuadTree* sw;
    QuadTree* se;
    int depth;
    
    bool operator==(const QuadTreeKey& other) const {
        return depth == other.depth && nw == other.nw && ne == other.ne && sw == other.sw && se == other.se;
    }
};

struct QuadTreeHash {
    size_t operator()(const QuadTreeKey& k) const {
        size_t seed = 0;
    
        auto hash_combine = [&](size_t h) {
            seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
    
        hash_combine(std::hash<int>{}(k.depth));
        hash_combine(std::hash<const QuadTree*>{}(k.nw));
        hash_combine(std::hash<const QuadTree*>{}(k.ne));
        hash_combine(std::hash<const QuadTree*>{}(k.sw));
        hash_combine(std::hash<const QuadTree*>{}(k.se));
    
        return seed;
    }
};

LRU<QuadTreeKey, std::shared_ptr<QuadTree>, QuadTreeHash> evolutionCache(evolutionCacheCapacity);
LRU<QuadTreeKey, std::shared_ptr<QuadTree>, QuadTreeHash> treeCache(treeCacheCapacity);
std::vector<std::shared_ptr<QuadTree>> leafCache;

/* ---------- Timer ---------- */

// https://www.learncpp.com/cpp-tutorial/timing-your-code/

class Timer {
private:
	// Type aliases to make accessing nested type easier
	using Clock = std::chrono::steady_clock;
	using Second = std::chrono::duration<double, std::ratio<1> >;

	std::chrono::time_point<Clock> m_beg { Clock::now() };

public:
	void reset() {
		m_beg = Clock::now();
	}

	double elapsed() const {
		return std::chrono::duration_cast<Second>(Clock::now() - m_beg).count();
	}
	
	void printElapsed() const { 
    double miliseconds {1000 * this->elapsed()}; 
    
    double seconds {miliseconds / 1000.0};
    double minutes {seconds / 60.0};
    double hours {minutes / 60.0};

    if (seconds >= 1.0) { 
        if (minutes >= 1.0) { 
            if (hours >= 1.0) { 
                std::cout << hours << "h"; 
            } else { 
                std::cout << minutes << "m"; 
            } 
        } else { 
            std::cout << seconds << "s"; 
        } 
    } else { 
        std::cout << miliseconds << "ms"; 
    } 
}
};

/* ---------- Quadtrees ---------- */

class QuadTree : public std::enable_shared_from_this<QuadTree> {
    public:
    std::shared_ptr<QuadTree> nw;
    std::shared_ptr<QuadTree> ne;
    std::shared_ptr<QuadTree> sw;
    std::shared_ptr<QuadTree> se;
    bool nw_leaf;
    bool ne_leaf;
    bool sw_leaf;
    bool se_leaf;
    int leaf_id;
    int depth;
    bool empty;
    std::shared_ptr<QuadTree> evolved;
    
    QuadTree(std::shared_ptr<QuadTree> nw, std::shared_ptr<QuadTree> ne, std::shared_ptr<QuadTree> sw, std::shared_ptr<QuadTree> se) {
        assert(nw->depth == ne->depth && ne->depth == sw->depth && sw->depth == se->depth && se->depth == nw->depth && "Depths are not matching");
        this->nw = nw; this->ne = ne;
        this->sw = sw; this->se = se;
        this->depth = nw->depth + 1;
        this->empty = nw->empty && ne->empty && sw->empty && se->empty;
    }
    QuadTree(bool nw, bool ne, bool sw, bool se) {
        this->nw_leaf = nw; this->ne_leaf = ne;
        this->sw_leaf = sw; this->se_leaf = se;
        this->depth = 1;
        this->leaf_id = nw + 2*ne + 4*sw + 8*se;
        this->empty = !(nw || ne || sw || se);
    }
    std::shared_ptr<QuadTree> addPadding() {
        /*
        PD PD PD PD
        PD nw ne PD
        PD sw ne PD
        PD PD PD PD
        */
        auto makePadding = [this] () {
            return createEmptyQuadTree(this->depth-1);
        };
        
        auto nw = createQuadTree(
            makePadding(), makePadding(),
            makePadding(), this->nw
        );
        auto ne = createQuadTree(
            makePadding(), makePadding(),
            this->ne, makePadding()
        );
        auto sw = createQuadTree(
            makePadding(), this->sw,
            makePadding(), makePadding()
        );
        auto se = createQuadTree(
            this->se, makePadding(),
            makePadding(), makePadding()
        );
        return createQuadTree(nw, ne, sw, se);
    }
    std::shared_ptr<QuadTree> addPadding(int count) {
        assert(count > 0);
        
        auto tree = shared_from_this();
        for (int i = 0; i < count; i++) {
            tree = tree->addPadding();
        }
        return tree;
    }
    std::shared_ptr<QuadTree> getCenter() {
        assert(this->depth >= 2 && "Depth is not enough to find center");
        if (depth == 2) {
            return createQuadTree(
                this->nw->se_leaf, this->ne->sw_leaf,
                this->sw->ne_leaf, this->se->nw_leaf
            );
        } else {
            return createQuadTree(
                this->nw->se, this->ne->sw,
                this->sw->ne, this->se->nw
            );
        }
    }
    
    bool isLeftEmpty() {
        if (this->depth == 1) return !this->nw_leaf && !this->sw_leaf;
        return this->nw->isLeftEmpty() && this->sw->isLeftEmpty();
    }
    bool isRightEmpty() { 
        if (this->depth == 1) {
            return !this->ne_leaf && !this->se_leaf; 
        }
        return this->ne->isRightEmpty() && this->se->isRightEmpty(); 
    }
    bool isTopEmpty() { 
        if (this->depth == 1) {
            return !this->nw_leaf && !this->ne_leaf; 
        }
        return this->nw->isTopEmpty() && this->ne->isTopEmpty(); 
    }
    bool isBottomEmpty() { 
        if (this->depth == 1) {
            return !this->sw_leaf && !this->se_leaf; 
        }
        return this->sw->isBottomEmpty() && this->se->isBottomEmpty(); 
    }
    bool isBorderEmpty() {
        return this->isLeftEmpty() && this->isRightEmpty() && this->isTopEmpty() && this->isBottomEmpty();
    }
    
    bool isPaddingEmpty() {
        /* padding shown as P
        P P P P
        P . . P
        P . . P
        P P P P
        */
        assert(this->depth >= 2 && "Depth is too low to call isPaddingEmpty");
        if (this->depth == 2) {
            return !(this->nw->nw_leaf || this->nw->ne_leaf || this->ne->nw_leaf || this->ne->ne_leaf ||
                     this->nw->sw_leaf/*this->nw->se_leaf || this->ne->sw_leaf*/ || this->ne->se_leaf ||
                     this->sw->nw_leaf/*this->sw->ne_leaf || this->se->nw_leaf*/ || this->se->ne_leaf ||
                     this->sw->sw_leaf || this->sw->se_leaf || this->se->sw_leaf || this->se->se_leaf);
        }
        return this->nw->nw->empty && this->nw->ne->empty && this->ne->nw->empty && this->ne->ne->empty && 
               this->nw->sw->empty/*this->nw->se->empty && this->ne->sw->empty*/ && this->ne->se->empty && 
               this->sw->nw->empty/*this->sw->ne->empty && this->se->nw->empty*/ && this->se->ne->empty && 
               this->sw->sw->empty && this->sw->se->empty && this->se->sw->empty && this->se->se->empty;
    }
    
    std::shared_ptr<QuadTree> trim() {
        if (this->depth <= 2) {
            return shared_from_this();
        }
        bool nwEmpty {this->nw->empty};
        bool neEmpty {this->ne->empty};
        bool swEmpty {this->sw->empty};
        bool seEmpty {this->se->empty};
        if (           neEmpty && swEmpty && seEmpty) return this->nw->trim();
        if (nwEmpty &&            swEmpty && seEmpty) return this->ne->trim();
        if (nwEmpty && neEmpty &&            seEmpty) return this->sw->trim();
        if (nwEmpty && neEmpty && swEmpty           ) return this->se->trim();
        
        return shared_from_this();
    }
    
    std::shared_ptr<QuadTree> evolveCenter() {
        if (this->evolved) {
            return this->evolved;
        }
        
        if (this->depth == 2) {
            /*
            nw.nw nw.ne ne.nw ne.ne
            nw.sw nw.se ne.sw ne.se
            sw.nw sw.ne se.nw se.ne
            sw.sw sw.se se.sw se.se
            */
            int neighbors_nw {
                this->nw->nw_leaf + this->nw->ne_leaf + this->ne->nw_leaf +
                this->nw->sw_leaf + /*-------------*/   this->ne->sw_leaf +
                this->sw->nw_leaf + this->sw->ne_leaf + this->se->nw_leaf
            };
            int neighbors_ne { 
                this->nw->ne_leaf + this->ne->nw_leaf + this->ne->ne_leaf +
                this->nw->se_leaf + /*-------------*/   this->ne->se_leaf +
                this->sw->ne_leaf + this->se->nw_leaf + this->se->ne_leaf
            };
            int neighbors_sw { 
                this->nw->sw_leaf + this->nw->se_leaf + this->ne->sw_leaf +
                this->sw->nw_leaf + /*-------------*/   this->se->nw_leaf +
                this->sw->sw_leaf + this->sw->se_leaf + this->se->sw_leaf
            };
            int neighbors_se { 
                this->nw->se_leaf + this->ne->sw_leaf + this->ne->se_leaf +
                this->sw->ne_leaf + /*-------------*/   this->se->ne_leaf +
                this->sw->se_leaf + this->se->sw_leaf + this->se->se_leaf
            };
            /*
            (live && neighbors == 2) || neighbors == 3
            */
            return this->evolved = createQuadTree(
                (this->nw->se_leaf && neighbors_nw == 2) || (neighbors_nw == 3),
                (this->ne->sw_leaf && neighbors_ne == 2) || (neighbors_ne == 3),
                (this->sw->ne_leaf && neighbors_sw == 2) || (neighbors_sw == 3),
                (this->se->nw_leaf && neighbors_se == 2) || (neighbors_se == 3)
            );
        } else {
            /*
              n
            w # e
              s
            
            * * * *
            * * * *
            * * * *
            * * * *
            */
            auto aux_nw {this->nw};
            auto aux_ne {this->ne};
            auto aux_sw {this->sw};
            auto aux_se {this->se};
            auto aux_n      = createQuadTree(this->nw->ne, this->ne->nw, this->nw->se, this->ne->sw);
            auto aux_e      = createQuadTree(this->ne->sw, this->ne->se, this->se->nw, this->se->ne);
            auto aux_s      = createQuadTree(this->sw->ne, this->se->nw, this->sw->se, this->se->sw);
            auto aux_w      = createQuadTree(this->nw->sw, this->nw->se, this->sw->nw, this->sw->ne);
            auto aux_center = createQuadTree(this->nw->se, this->ne->sw, this->sw->ne, this->se->nw);
            
            aux_nw = aux_nw->evolveCenter();
            aux_n = aux_n->evolveCenter();
            aux_ne = aux_ne->evolveCenter();
            aux_e = aux_e->evolveCenter();
            aux_se = aux_se->evolveCenter();
            aux_s = aux_s->evolveCenter();
            aux_sw = aux_sw->evolveCenter();
            aux_w = aux_w->evolveCenter();
            aux_center = aux_center->evolveCenter();
            
            auto largeAux_nw = createQuadTree(aux_nw, aux_n, aux_w, aux_center);
            auto largeAux_ne = createQuadTree(aux_n, aux_ne, aux_center, aux_e);
            auto largeAux_sw = createQuadTree(aux_w, aux_center, aux_sw, aux_s);
            auto largeAux_se = createQuadTree(aux_center, aux_e, aux_s, aux_se);
            
            largeAux_nw = largeAux_nw->evolveCenter();
            largeAux_ne = largeAux_ne->evolveCenter();
            largeAux_sw = largeAux_sw->evolveCenter();
            largeAux_se = largeAux_se->evolveCenter();
            
            return this->evolved = createQuadTree(
                largeAux_nw,
                largeAux_ne,
                largeAux_sw,
                largeAux_se
            );
        }
    }
};

std::shared_ptr<QuadTree> createQuadTree(const std::shared_ptr<QuadTree> nw, const std::shared_ptr<QuadTree> ne, const std::shared_ptr<QuadTree> sw, const std::shared_ptr<QuadTree> se) {
    QuadTreeKey treeKey {nw.get(), ne.get(), sw.get(), se.get(), nw->depth};
    
    if (treeCache.contains(treeKey)) return treeCache[treeKey];
    
    return treeCache[treeKey] = std::make_shared<QuadTree>(nw, ne, sw, se);
}

std::shared_ptr<QuadTree> createQuadTree(bool nw, bool ne, bool sw, bool se) {
    int hashedKey {nw + 2*ne + 4*sw + 8*se};
    
    return leafCache[hashedKey];
}

std::shared_ptr<QuadTree> createQuadTree(const std::shared_ptr<QuadTree> same) {return createQuadTree(same, same, same, same);}
std::shared_ptr<QuadTree> createQuadTree(bool same) {return createQuadTree(same, same, same, same);}

/* ---------- Helper Functions ---------- */

bool isPowerOfTwo(int n) { // magic function that does something and somehow works
    return n > 0 && (n & (n - 1)) == 0;
}

int nextHighestPowerOf2(int v) {
    // magic function taken from stackoverflow that does something and somehow works
    
    // Source - https://stackoverflow.com/a/466242
    // Posted by florin, modified by community. See post 'Timeline' for change history
    // Retrieved 2026-09-03, License - CC BY-SA 4.0
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return ++v;
}

int power2(int x) {
    return 1 << x;
}

std::shared_ptr<QuadTree> createEmptyQuadTree(int depth) {
    static std::vector<std::shared_ptr<QuadTree>> emptyTrees;
    if (emptyTrees.empty()) emptyTrees.push_back(createQuadTree(false));
    while (static_cast<int>(emptyTrees.size()) < depth) {
        emptyTrees.push_back(createQuadTree(emptyTrees.back())); // emptyTrees[size-1]
    }
    return emptyTrees[depth-1];
}

std::vector<std::vector<bool>> join2x2Arrays(std::vector<std::vector<bool>>& nw, std::vector<std::vector<bool>>& ne, std::vector<std::vector<bool>>& sw, std::vector<std::vector<bool>>& se) {
    assert(nw.size() == nw[0].size() && "Size not square (nw)");
    assert(ne.size() == ne[0].size() && "Size not square (ne)");
    assert(sw.size() == sw[0].size() && "Size not square (sw)");
    assert(se.size() == se[0].size() && "Size not square (se)");
    assert(nw.size() == ne.size() && ne.size() == sw.size() && sw.size() == se.size() && se.size() == nw.size() && "Sizes not matching");
    int size {static_cast<int>(nw.size())};
    std::vector<std::vector<bool>> finalVector(size*2, std::vector<bool>(size*2));
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            finalVector[i][j]           = nw[i][j];
            finalVector[i+size][j]      = ne[i][j];
            finalVector[i][j+size]      = sw[i][j];
            finalVector[i+size][j+size] = se[i][j];
        }
    }
    return finalVector;
}

/* ---------- Array <--> quadtree conversions ---------- */

std::shared_ptr<QuadTree> squareArrayToQuadTreeHelper(const std::vector<std::vector<bool>>& grid, int minX, int minY, int maxX, int maxY) {
    int size = maxX - minX;
    if (size == 2) {
        return createQuadTree(
            grid[minX][minY],   grid[minX+1][minY], 
            grid[minX][minY+1], grid[minX+1][minY+1]
        );
    }
    
    int midX = minX + size/2;
    int midY = minY + size/2;
    return createQuadTree(
        squareArrayToQuadTreeHelper(grid, minX, minY, midX, midY),
        squareArrayToQuadTreeHelper(grid, midX, minY, maxX, midY),
        squareArrayToQuadTreeHelper(grid, minX, midY, midX, maxY),
        squareArrayToQuadTreeHelper(grid, midX, midY, maxX, maxY)
    );
}

std::shared_ptr<QuadTree> squareArrayToQuadTree(const std::vector<std::vector<bool>>& grid) {
    assert(grid.size() > 0 && "Grid is empty");
    assert(grid.size() == grid[0].size() && "Grid needs to be square");
    assert(isPowerOfTwo(grid.size()) && "Grid size needs to be a power of 2");
    // todo: handle 1x1, 2x2
    int size {static_cast<int>(grid.size())};
    
    return squareArrayToQuadTreeHelper(grid, 0, 0, size, size);
}

std::vector<std::vector<bool>> quadTreeToArray(const std::shared_ptr<QuadTree> quadTree) {
    int size {quadTree->depth};
    if (size == 1) {
        std::vector<std::vector<bool>> finalVector(2, std::vector<bool>(2));
        finalVector[0][0] = quadTree->nw_leaf;
        finalVector[1][0] = quadTree->ne_leaf;
        finalVector[0][1] = quadTree->sw_leaf;
        finalVector[1][1] = quadTree->se_leaf;
        return finalVector;
    }
    std::vector<std::vector<bool>> nw {quadTreeToArray(quadTree->nw)};
    std::vector<std::vector<bool>> ne {quadTreeToArray(quadTree->ne)};
    std::vector<std::vector<bool>> sw {quadTreeToArray(quadTree->sw)};
    std::vector<std::vector<bool>> se {quadTreeToArray(quadTree->se)};
    
    return join2x2Arrays(nw, ne, sw, se);
}

std::shared_ptr<QuadTree> arrayToQuadTree(const std::vector<std::vector<bool>>& grid) {
    int sizeX {static_cast<int>(grid.size())};
    int sizeY {static_cast<int>(grid[0].size())};
    int idealSize {nextHighestPowerOf2(std::max(sizeX, sizeY))};
    
    std::vector<std::vector<bool>> gridCopy(grid);
    
    gridCopy.resize(idealSize);
    for (auto& row : gridCopy) {
        row.resize(idealSize, false);
    }
    
    return squareArrayToQuadTree(gridCopy);
}

/* ---------- I/O ----------- */

void printArray(std::vector<std::vector<bool>>& array, char char_live = '#', char char_dead = '.', bool crop = true) {
    std::stringstream ss;
    
    int minCol {0};
    int minRow {0};
    int sizeRow {static_cast<int>(array.size())};
    int sizeCol {static_cast<int>(array[0].size())};
    int maxCol {sizeCol-1};
    int maxRow {sizeRow-1};
    
    auto isRowEmpty = [&array, sizeCol](int idx) -> bool {
        for (int i = 0; i < sizeCol; i++) {
            if (array[idx][i]) return false;
        }
        return true;
    };
    auto isColEmpty = [&array, sizeRow](int idx) -> bool {
        for (int i = 0; i < sizeRow; i++) {
            if (array[i][idx]) return false;
        }
        return true;
    };
    
    if (crop) {
        for (int i = 0; i < sizeCol; i++) {
            if (!isColEmpty(i)) {
                minCol = std::max(0,i-1);
                break;
            }
        }
        for (int i = sizeCol-1; i >= 0; i--) {
            if (!isColEmpty(i)) {
                maxCol = std::min(sizeCol-1,i+1);
                break;
            }
        }
        for (int i = 0; i < sizeRow; i++) {
            if (!isRowEmpty(i)) {
                minRow = std::max(0,i-1);
                break;
            }
        }
        for (int i = sizeRow-1; i >= 0; i--) {
            if (!isRowEmpty(i)) {
                maxRow = std::min(sizeRow-1,i+1);
                break;
            }
        }
    }
    
    for (int i = minRow; i <= maxRow; i++) {
        for (int j = minCol; j <= maxCol; j++) {
            ss << (array[i][j] ? char_live : char_dead) << " ";
        }
        ss << "\n";
    }
    
    ss << "\n";
    
    std::cout << ss.str();
}
void printQuadTree(const std::shared_ptr<QuadTree> quadTree) {
    std::vector<std::vector<bool>> array {quadTreeToArray(quadTree)};
    printArray(array); // todo: possibly memory inefficient
}
bool isDigit(char x) {
    return x == '0' || x == '1' || x == '2' || x == '3' || x == '4' || x == '5' || x == '6' || x == '7' || x == '8' || x == '9';
}

std::vector<std::vector<bool>> parseRLE(std::string_view rle) {
    // does not support comments or headers
    int i {0};
    std::vector<std::vector<bool>> result;
    result.emplace_back();
    int maxRowLength {0};
    int currentRowLength {0};
    int rleLength {static_cast<int>(rle.length())};
    while (i < rleLength) {
        std::string strCoefficient {""};
        while (i < rleLength && isDigit(rle[i])) {
            strCoefficient += rle[i];
            i++;
        }
        int coefficient {strCoefficient == "" ? 1 : std::stoi(strCoefficient)};
        if (rle[i] == 'b' || rle[i] == 'o') {
            for (int j = 0; j < coefficient; j++) {
                result.back().push_back(rle[i] == 'o');
                currentRowLength++;
            }
        } else if (rle[i] == '$') {
            for (int j = 0; j < coefficient; j++) {
                result.emplace_back();
            }
            currentRowLength = 0;
        }
        if (currentRowLength > maxRowLength) maxRowLength = currentRowLength;
        i++;
    }
    
    for (auto& row : result) {
        row.resize(maxRowLength);
    }
    return result;
}

/* ---------- Init -----------*/

void init() {
    leafCache.resize(16);
    for (int i = 0; i <= 15; i++) {
        leafCache[i] = std::make_shared<QuadTree>(i & 1, (i >> 1) & 1, (i >> 2) & 1, (i >> 3) & 1);
    }
}

/* ---------- Main ----------- */

int main() {
    init();
    std::vector<std::vector<bool>> array {parseRLE("o5bob$2bo3bob$2bo2bobo$bobo!")};
    constexpr bool doPrint {0};
    auto tree {arrayToQuadTree(array)->trim()};
    printQuadTree(tree);
    
    Timer timer {};
    timer.reset();
    int i {0};
    while (i < 1'000'000'000) {
        while (!tree->isPaddingEmpty()) {
            tree = tree->addPadding();
        }
        i += power2(tree->depth-1);
        tree = tree->addPadding()->evolveCenter()->trim();
        std::cout << "Generation #" << i << ":\n";
        // printQuadTree(tree);
    }
    timer.printElapsed();
    std::cout << " elapsed\n";
    // printQuadTree(tree);
    return 0;
}


