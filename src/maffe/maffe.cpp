#include "maffe.hpp"
#include "config.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "maffe/common.hpp"
#include "reductions/reductions.hpp"
#include "newick/newick.hpp"
#include "util/log.hpp"

namespace maffe {
namespace {
struct ParsedTrees {
    std::vector<newick::Tree> trees;
    std::vector<std::string> leaf_names;
};

std::vector<std::string> leaf_labels(const newick::Tree& tree) {
    std::vector<std::string> labels;
    const auto walk = [&](this auto&& self, const int node) -> void {
        const auto& [children, label] = tree.nodes[node];
        if (children.empty()) {
            if (label.empty())
                throw std::invalid_argument("empty leaf label");
            labels.push_back(label);
            return;
        }

        if (children.size() != 2)
            throw std::invalid_argument("input trees must be rooted and binary");

        self(children[0]);
        self(children[1]);
    };

    walk(tree.root);
    return labels;
}

std::unordered_map<std::string, int> leaf_index(const std::vector<std::string>& labels) {
    std::unordered_map<std::string, int> index;
    index.reserve(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (!index.emplace(labels[i], static_cast<int>(i)).second)
            throw std::invalid_argument("duplicate leaf label");
    }
    return index;
}

std::string format_seconds(const double seconds) {
    return std::format("{:.3f}s", seconds);
}

Tree annotate_tree(const newick::Tree& input, const std::unordered_map<std::string, int>& leaf_index) {
    Tree tree{
        .parent = std::vector<int>(input.nodes.size(), -2),
        .edge_state = std::vector<EdgeState>(input.nodes.size(), EdgeState::UNKNOWN),
    };
    std::vector<int> index(input.nodes.size(), -1);
    int next = static_cast<int>(leaf_index.size());

    const auto visit = [&](this auto&& self, const int node) -> void {
        const auto& [children, label] = input.nodes[node];
        if (children.empty()) {
            const auto it = leaf_index.find(label);
            if (it == leaf_index.end())
                throw std::invalid_argument("leaf set mismatch between trees");
            index[node] = it->second;
            return;
        }

        if (children.size() != 2)
            throw std::invalid_argument("input trees must be rooted and binary");

        for (const auto child : children)
            self(child);

        const int parent = next++;
        for (const auto child : children)
            tree.parent[index[child]] = parent;
        index[node] = parent;
    };

    visit(input.root);
    tree.parent[index[input.root]] = -1;
    return tree;
}

class RestrictionBuilder {
public:
    RestrictionBuilder(const newick::Tree& source_tree, const std::vector<std::string>& leaf_names)
        : source(source_tree) {
        const int nodes = static_cast<int>(source.nodes.size());
        if (source.root < 0 || source.root >= nodes)
            throw std::invalid_argument("invalid source tree");

        int levels = 1;
        while ((1 << levels) <= std::max(nodes, 1))
            ++levels;

        tin.resize(nodes, -1);
        tout.resize(nodes, -1);
        up.assign(levels, std::vector<int>(nodes, 0));
        leaf_node.reserve(leaf_names.size());
        build_index(source.root, -1);
    }

    [[nodiscard]] std::string restrict_block(const std::vector<std::string>& leaves) const {
        if (leaves.empty())
            throw std::invalid_argument("subtree needs at least one leaf");

        std::vector<int> selected;
        selected.reserve(leaves.size());
        for (const auto& leaf : leaves) {
            const auto it = leaf_node.find(leaf);
            if (it == leaf_node.end())
                throw std::invalid_argument("unknown leaf name");
            selected.push_back(it->second);
        }

        std::ranges::sort(selected, {}, [&](const int node) { return tin[node]; });
        selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
        if (selected.empty())
            throw std::invalid_argument("subtree selection is empty");
        if (selected.size() == 1)
            return source.nodes[selected.front()].label + ";";

        std::vector<int> virtual_nodes = selected;
        virtual_nodes.reserve(selected.size() * 2);
        for (int i = 1; i < static_cast<int>(selected.size()); ++i)
            virtual_nodes.push_back(lca(selected[i - 1], selected[i]));

        std::ranges::sort(virtual_nodes, {}, [&](const int node) { return tin[node]; });
        virtual_nodes.erase(std::unique(virtual_nodes.begin(), virtual_nodes.end()), virtual_nodes.end());

        std::unordered_map<int, int> local;
        local.reserve(virtual_nodes.size());
        for (int i = 0; i < static_cast<int>(virtual_nodes.size()); ++i)
            local.emplace(virtual_nodes[i], i);

        std::vector<std::vector<int>> children(virtual_nodes.size());
        std::vector<int> stack;
        stack.reserve(virtual_nodes.size());
        stack.push_back(virtual_nodes.front());
        for (int i = 1; i < static_cast<int>(virtual_nodes.size()); ++i) {
            const int node = virtual_nodes[i];
            while (stack.size() >= 2 && !is_ancestor(stack.back(), node)) {
                const int child = stack.back();
                stack.pop_back();
                children[local.at(stack.back())].push_back(local.at(child));
            }
            stack.push_back(node);
        }
        while (stack.size() > 1) {
            const int child = stack.back();
            stack.pop_back();
            children[local.at(stack.back())].push_back(local.at(child));
        }

        return build_newick(local.at(stack.back()), virtual_nodes, children) + ";";
    }

private:
    void build_index(const int node, const int parent) {
        tin[node] = timer++;
        up[0][node] = parent < 0 ? node : parent;
        for (int level = 1; level < static_cast<int>(up.size()); ++level)
            up[level][node] = up[level - 1][up[level - 1][node]];

        const auto& [children, label] = source.nodes[node];
        if (children.empty()) {
            if (!leaf_node.emplace(label, node).second)
                throw std::invalid_argument("duplicate leaf label");
        } else {
            if (children.size() != 2)
                throw std::invalid_argument("input trees must be rooted and binary");
            build_index(children[0], node);
            build_index(children[1], node);
        }

        tout[node] = timer;
    }

    [[nodiscard]] bool is_ancestor(const int ancestor, const int node) const {
        return tin[ancestor] <= tin[node] && tout[node] <= tout[ancestor];
    }

    [[nodiscard]] int lca(int a, const int b) const {
        if (is_ancestor(a, b))
            return a;
        if (is_ancestor(b, a))
            return b;
        for (int level = static_cast<int>(up.size()) - 1; level >= 0; --level) {
            const int parent = up[level][a];
            if (!is_ancestor(parent, b))
                a = parent;
        }
        return up[0][a];
    }

    [[nodiscard]] std::string build_newick(
        const int node,
        const std::vector<int>& virtual_nodes,
        const std::vector<std::vector<int>>& children
    ) const {
        const auto& local_children = children[node];
        if (local_children.empty())
            return source.nodes[virtual_nodes[node]].label;
        if (local_children.size() == 1)
            return build_newick(local_children.front(), virtual_nodes, children);
        return "(" + build_newick(local_children[0], virtual_nodes, children) + "," +
            build_newick(local_children[1], virtual_nodes, children) + ")";
    }

    const newick::Tree& source;
    std::unordered_map<std::string, int> leaf_node;
    std::vector<int> tin;
    std::vector<int> tout;
    std::vector<std::vector<int>> up;
    int timer = 0;
};

ParsedTrees parse_trees(const std::vector<std::string>& input) {
    if (input.empty())
        throw std::invalid_argument("instance must contain at least one tree");

    ParsedTrees parsed;
    parsed.trees.reserve(input.size());
    std::unordered_map<std::string, int> index;
    for (const auto& line : input) {
        auto tree = newick::parse(line);
        auto labels = leaf_labels(tree);
        auto labels_index = leaf_index(labels);

        if (parsed.trees.empty()) {
            if (labels.empty())
                throw std::invalid_argument("trees must contain at least one leaf");
            index = std::move(labels_index);
            parsed.leaf_names = std::move(labels);
        } else if (labels_index.size() != index.size()) {
            throw std::invalid_argument("all trees must have the same leaf set");
        } else {
            for (const auto& leaf : labels) {
                if (!index.contains(leaf))
                    throw std::invalid_argument("all trees must have the same leaf set");
            }
        }

        parsed.trees.push_back(std::move(tree));
    }

    return parsed;
}

} // namespace

std::vector<std::vector<std::string>> solve(const std::vector<std::string>& trees) {
    return solve(trees, {});
}

std::vector<std::vector<std::string>> solve(const std::vector<std::string>& trees, const SolveOptions& options) {
    const ParsedTrees parsed = parse_trees(trees);
    const auto start = std::chrono::steady_clock::now();
    constexpr std::string_view git_hash = MAFFE_GIT_HASH;
    if (logging::enabled(options.log_level)) {
        logging::line(
            "solver: maffe ", MAFFE_PROJECT_VERSION,
            " git=", git_hash.substr(0, std::min<std::size_t>(git_hash.size(), 6)),
            MAFFE_GIT_DIRTY ? "-dirty" : "",
            " scip=", MAFFE_SCIP_VERSION,
            " lps=", MAFFE_SCIP_LPS,
            " highs=", MAFFE_HIGHS_VERSION
        );
        logging::line("instance: trees=", parsed.trees.size(), " leaves=", parsed.leaf_names.size());
        if (options.timeout_seconds.has_value())
            logging::line("limit: time-s=", *options.timeout_seconds);
        if (options.acceptable_factor != 1.0 || options.acceptable_offset != 0) {
            logging::line(
                "accept: factor=", options.acceptable_factor,
                " offset=", options.acceptable_offset
            );
        }
    }

    AnnotatedInstance annotated;
    annotated.trees.reserve(parsed.trees.size());

    const auto index = leaf_index(parsed.leaf_names);
    for (const auto& tree : parsed.trees)
        annotated.trees.push_back(annotate_tree(tree, index));

    const auto partition = solve_annotated(
        std::move(annotated),
        options.timeout_seconds,
        options.acceptable_factor,
        options.acceptable_offset,
        options.log_level,
        options.heuristic_mode
    );

    std::vector<std::vector<std::string>> solution;
    solution.reserve(partition.partition.size());
    for (const auto& block : partition.partition) {
        std::vector<std::string> leaves;
        leaves.reserve(block.size());
        for (const auto leaf : block)
            leaves.push_back(parsed.leaf_names.at(leaf));
        solution.push_back(std::move(leaves));
    }
    if (logging::enabled(options.log_level)) {
        logging::line("result: agreement forest");
        logging::line("result:   components=", solution.size());
        logging::line(
            "result:   time=",
            format_seconds(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count())
        );
    }
    return solution;
}

std::string restrict(const std::vector<std::string>& trees, const std::vector<std::string>& leaves) {
    const ParsedTrees parsed = parse_trees(trees);
    return RestrictionBuilder(parsed.trees.front(), parsed.leaf_names).restrict_block(leaves);
}

std::vector<std::string> restrict(
    const std::vector<std::string>& trees,
    const std::vector<std::vector<std::string>>& partition
) {
    const ParsedTrees parsed = parse_trees(trees);
    RestrictionBuilder builder(parsed.trees.front(), parsed.leaf_names);
    std::vector<std::string> output;
    output.reserve(partition.size());
    for (const auto& leaves : partition)
        output.push_back(builder.restrict_block(leaves));
    return output;
}

} // namespace maffe
