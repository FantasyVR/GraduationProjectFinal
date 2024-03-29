
#include "BoundingSphereHierarchy.h"
#include "MiniBall.h"

#include <iostream>
#include <unordered_set>
#include <set>

using namespace Eigen;
using pool_set = std::set<unsigned int>;
using namespace PBD;


PointCloudBSH::PointCloudBSH()
    : super(0, 10)
{
}

Vector3r const& PointCloudBSH::entity_position(unsigned int i) const
{
    return m_vertices[i];
}

struct PCBSHCoordAccessor
{
    PCBSHCoordAccessor(const Vector3r *vertices_,
        std::vector<unsigned int> const* lst_)
        : vertices(vertices_), lst(lst_)
    {
    }

    using Pit = unsigned int;
    using Cit = Real const*;
    inline  Cit operator() (Pit it) const {
        return vertices[(*lst)[it]].data();
    }
    const Vector3r *vertices;
    std::vector<unsigned int> const* lst;
};


void PointCloudBSH::compute_hull(unsigned int b, unsigned int n, BoundingSphere& hull) const
{
    auto mb = Miniball::Miniball<PCBSHCoordAccessor>(3, b, b + n,  {m_vertices, &m_lst});

    hull.x() = Map<Vector3r const>(mb.center());
    hull.r() = std::sqrt(mb.squared_radius());
}

void PointCloudBSH::compute_hull_approx(unsigned int b, unsigned int n, BoundingSphere& hull) const
{
	// compute center
	Vector3r x;
	x.setZero();
	for (unsigned int i = b; i < b+n; i++)
		x += m_vertices[m_lst[i]];
	x /= (Real)n;

	Real radius2 = 0.0;
	for (unsigned int i = b; i < b+n; i++)
	{
		radius2 = std::max(radius2, (x - m_vertices[m_lst[i]]).squaredNorm());
	}

	hull.x() = x;
	hull.r() = sqrt(radius2);
}

void PointCloudBSH::init(const Vector3r *vertices, const unsigned int numVertices)
{
	m_lst.resize(numVertices);
	m_vertices = vertices;
	m_numVertices = numVertices;
}

//////////////////////////////////////////////////////////////////////////


struct TMBSHCoordAccessor
{
	TMBSHCoordAccessor(const Vector3r *vertices_, const unsigned int *indices_, 
		std::vector<unsigned int> const* lst_)
		: vertices(vertices_), lst(lst_)
	{
	}

	using Pit = unsigned int;
	using Cit = Real const*;
	inline  Cit operator() (Pit it) const {
		return vertices[(*lst)[it]].data();
	}
	const Vector3r *vertices;
	const unsigned int *indices;
	std::vector<unsigned int> const* lst;
};


TetMeshBSH::TetMeshBSH()
	: super(0)
{
}

Vector3r const& TetMeshBSH::entity_position(unsigned int i) const
{
	return m_com[i];
}

void TetMeshBSH::compute_hull(unsigned int b, unsigned int n, BoundingSphere& hull) const
{
	compute_hull_approx(b, n, hull);
}

void TetMeshBSH::compute_hull_approx(unsigned int b, unsigned int n, BoundingSphere& hull) const
{
	// compute center
	Vector3r x;
	x.setZero();
	for (unsigned int i = b; i < b + n; i++)
	{
		const unsigned int tet = m_lst[i];	
		x += m_vertices[m_indices[4 * tet]];
		x += m_vertices[m_indices[4 * tet + 1]];
		x += m_vertices[m_indices[4 * tet + 2]];
		x += m_vertices[m_indices[4 * tet + 3]];
	}
	x /= ((Real)4.0* (Real)n);

	Real radius2 = 0.0;
	for (unsigned int i = b; i < b + n; i++)
	{
		const unsigned int tet = m_lst[i];
		radius2 = std::max(radius2, (x - m_vertices[m_indices[4 * tet]]).squaredNorm());
		radius2 = std::max(radius2, (x - m_vertices[m_indices[4 * tet+1]]).squaredNorm());
		radius2 = std::max(radius2, (x - m_vertices[m_indices[4 * tet+2]]).squaredNorm());
		radius2 = std::max(radius2, (x - m_vertices[m_indices[4 * tet+3]]).squaredNorm());
	}

	hull.x() = x;
	hull.r() = sqrt(radius2) + m_tolerance;
}

void TetMeshBSH::init(const Vector3r *vertices, const unsigned int numVertices, const unsigned int *indices, const unsigned int numTets, const Real tolerance)
{
	m_lst.resize(numTets);
	m_vertices = vertices;
	m_numVertices = numVertices;
	m_indices = indices;
	m_numTets = numTets;
	m_tolerance = tolerance;
	m_com.resize(numTets);
	for (unsigned int i = 0; i < numTets; i++)
	{
		m_com[i] = 0.25 * (m_vertices[m_indices[4*i]] + m_vertices[m_indices[4*i+1]] + m_vertices[m_indices[4*i+2]] + m_vertices[m_indices[4*i+3]]);
	}
}



void PBD::BVHTest::t_single(PointCloudBSH const & b1, TraversalCallback func)
{
	t_single(b1, 0, 0, func);
}

void PBD::BVHTest::t_single(PointCloudBSH const & b1, const unsigned int node_index1, const unsigned int node_index2, TraversalCallback func)
{
	const BoundingSphere &bs1 = b1.hull(node_index1);
	const BoundingSphere &bs2 = b1.hull(node_index2);
	auto const& node1 = b1.node(node_index1);
	auto const& node2 = b1.node(node_index2);
	
	if(!node1.is_leaf())
	{
		t_single(b1, node1.children[0], node_index2, func);
		t_single(b1, node1.children[1], node_index2, func);
		return;
	}
	if (node_index1 != node_index2)
	{
		if (!bs1.overlaps(bs2))
			return;
	}
	if (node1.is_leaf() && node2.is_leaf())
	{
		if (node_index1 == node_index2)return;
		func(node_index1, node_index2);
	//	printf("%d %d\n",node_index1,node_index2);
		return;
	}
	t_single(b1, node_index1, node2.children[0], func);
	t_single(b1, node_index1, node2.children[1], func);
	
}

void BVHTest::traverse(PointCloudBSH const& b1, TetMeshBSH const& b2, TraversalCallback func)
{
	traverse(b1, 0, b2, 0, func);

}

void BVHTest::traverse(PointCloudBSH const& b1, const unsigned int node_index1, TetMeshBSH const& b2, const unsigned int node_index2, TraversalCallback func)
{
	const BoundingSphere &bs1 = b1.hull(node_index1);
	const BoundingSphere &bs2 = b2.hull(node_index2);
	if (!bs1.overlaps(bs2))
		return;

	auto const& node1 = b1.node(node_index1);
	auto const& node2 = b2.node(node_index2);
	if (node1.is_leaf() && node2.is_leaf())
	{
		func(node_index1, node_index2);
		return;
	}

	if (bs1.r() < bs2.r())
	{
		if (!node1.is_leaf())
		{
			traverse(b1, node1.children[0], b2, node_index2, func);
			traverse(b1, node1.children[1], b2, node_index2, func);
		}
		else
		{
			traverse(b1, node_index1, b2, node2.children[0], func);
			traverse(b1, node_index1, b2, node2.children[1], func);
		}
	}
	else
	{
		if (!node2.is_leaf())
		{
			traverse(b1, node_index1, b2, node2.children[0], func);
			traverse(b1, node_index1, b2, node2.children[1], func);
		}
		else
		{
			traverse(b1, node1.children[0], b2, node_index2, func);
			traverse(b1, node1.children[1], b2, node_index2, func);
		}
	}

}