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

/* ---------- Memoization ---------- */

struct QuadTreeKey {
    QuadTree* nw;
    QuadTree* ne;
    QuadTree* sw;
    QuadTree* se;
    int depth;
};

size_t hashKey(const QuadTreeKey& k) {
    // magic function that does something and somehow works
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
};

std::unordered_map<int, std::shared_ptr<QuadTree>> evolutionCache;

std::unordered_map<int, std::shared_ptr<QuadTree>> treeCache;

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
    
    QuadTree(std::shared_ptr<QuadTree> nw, std::shared_ptr<QuadTree> ne, std::shared_ptr<QuadTree> sw, std::shared_ptr<QuadTree> se) {
        assert(nw->depth == ne->depth && ne->depth == sw->depth && sw->depth == se->depth && se->depth == nw->depth && "Depths are not matching");
        this->nw = nw; this->ne = ne;
        this->sw = sw; this->se = se;
        this->depth = nw->depth + 1;
    }
    QuadTree(bool nw, bool ne, bool sw, bool se) {
        this->nw_leaf = nw; this->ne_leaf = ne;
        this->sw_leaf = sw; this->se_leaf = se;
        this->depth = 1;
        this->leaf_id = nw + 2*ne + 4*sw + 8*se;
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
        return std::make_shared<QuadTree>(nw, ne, sw, se);
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
    
    bool isEmpty() {
        if (this->depth == 1) return !(this->nw_leaf || this->ne_leaf || this->sw_leaf || this->se_leaf);
        return this->nw->isEmpty() && this->ne->isEmpty() && this->sw->isEmpty() && this->se->isEmpty();
    }
    
    std::shared_ptr<QuadTree> trim() {
        bool nwEmpty {this->nw->isEmpty()};
        bool neEmpty {this->ne->isEmpty()};
        bool swEmpty {this->sw->isEmpty()};
        bool seEmpty {this->se->isEmpty()};
        if (           neEmpty && swEmpty && seEmpty) return this->nw->trim();
        if (nwEmpty &&            swEmpty && seEmpty) return this->ne->trim();
        if (nwEmpty && neEmpty &&            seEmpty) return this->sw->trim();
        if (nwEmpty && neEmpty && swEmpty           ) return this->se->trim();
        
        return shared_from_this();
    }
    
    std::shared_ptr<QuadTree> evolveCenter() {
        /*
        Takes the currrent QuadTree:
        1 2 | 1 2
        3 4 | 3 4
        ----+----
        1 2 | 1 2
        3 4 | 3 4
        Returns the evolved center:
        * * | * *
        * 4 | 3 *
        ----+----
        * 2 | 1 *
        * * | * *
        */
        
        QuadTreeKey treeKey {this->nw.get(), this->ne.get(), this->sw.get(), this->se.get(), this->nw->depth};
        
        if (evolutionCache.contains(treeKey)) {
            return evolutionCache[treeKey];
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
            return evolutionCache[treeKey] = createQuadTree(
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
            
            largeAux_nw = largeAux_nw->getCenter();
            largeAux_ne = largeAux_ne->getCenter();
            largeAux_sw = largeAux_sw->getCenter();
            largeAux_se = largeAux_se->getCenter();
            
            return evolutionCache[treeKey] = createQuadTree(
                largeAux_nw,
                largeAux_ne,
                largeAux_sw,
                largeAux_se
            );
        }
    }
    std::shared_ptr<QuadTree> evolve() {
        auto trimmed {this->trim()};
        if (!this->isBorderEmpty()) {
            return trimmed->addPadding()->evolve();
        }
        return trimmed->addPadding()->evolveCenter();
    }
};

std::shared_ptr<QuadTree> createQuadTree(const std::shared_ptr<QuadTree> nw, const std::shared_ptr<QuadTree> ne, const std::shared_ptr<QuadTree> sw, const std::shared_ptr<QuadTree> se) {
    QuadTreeKey treeKey {nw.get(), ne.get(), sw.get(), se.get(), nw->depth};
    int hashedKey {static_cast<int>(hashKey(treeKey))};
    
    if (treeCache.contains(hashedKey)) return treeCache.at(hashedKey);
    
    return treeCache[hashedKey] = std::make_shared<QuadTree>(nw, ne, sw, se);
}

std::shared_ptr<QuadTree> createQuadTree(bool nw, bool ne, bool sw, bool se) {
    int hashedKey {nw + 2*ne + 4*sw + 8*se};
    
    return leafCache[hashedKey];
}

/* ---------- Quadtrees (inefficient, used for comparison) ---------- */
/*
std::shared_ptr<QuadTree> createQuadTree(const std::shared_ptr<QuadTree> nw, const std::shared_ptr<QuadTree> ne, const std::shared_ptr<QuadTree> sw, const std::shared_ptr<QuadTree> se) {
    return std::make_shared<QuadTree>(nw, ne, sw, se);
}

std::shared_ptr<QuadTree> createQuadTree(bool nw, bool ne, bool sw, bool se) {
    return std::make_shared<QuadTree>(nw, ne, sw, se);
}

/* ---------- Helper Functions ---------- */

bool isPowerOfTwo(int n) { // magic function that does something and somehow works
    return n > 0 && (n & (n - 1)) == 0;
}

std::shared_ptr<QuadTree> createEmptyQuadTree(int depth) {
    if (depth == 1) {
        return createQuadTree(false,false,false,false);
    }
    return createQuadTree(
        createEmptyQuadTree(depth-1),
        createEmptyQuadTree(depth-1),
        createEmptyQuadTree(depth-1),
        createEmptyQuadTree(depth-1)
    );
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

/* ---------- I/O ----------- */

void printArray(std::vector<std::vector<bool>>& array, char char_live = '#', char char_dead = '.') {
    std::stringstream ss;
    
    for (auto& i : array) {
        for (bool j : i) {
            ss << (j ? char_live : char_dead) << " ";
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

/* ---------- Init -----------*/

void init() {
    leafCache.reserve(16);
    for (int i = 0; i <= 15; i++) {
        leafCache[i] = std::make_shared<QuadTree>(i & 1, (i >> 1) & 1, (i >> 2) & 1, (i >> 3) & 1);
    }
}

/* ---------- Main ----------- */

int main() {
    init();
    std::vector<std::vector<bool>> array {{
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0}, 
        {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,1,0,0,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }};
    constexpr bool doPrint {false};
    auto tree = squareArrayToQuadTree(array);
    printQuadTree(tree);
    
    Timer timer {};
    timer.reset();
    for (int i = 0; i < 5300; i++) {
        tree = tree->evolve();
        if (doPrint) printQuadTree(tree);
        std::cout << "Completed generation #" << i << "\n";
    }
    std::cout << (timer.elapsed()*1000) << "ms elapsed";
    if (!doPrint) printQuadTree(tree);
    return 0;
}
