/*
 * mwcspreprocessruleposdeg01.h
 *
 *  Created on: 22-jan-2013
 *      Author: M. El-Kebir
 */

#ifndef MWCSPREPROCESSRULEPOSDEG01_H
#define MWCSPREPROCESSRULEPOSDEG01_H

#include <lemon/core.h>
#include <string>
#include <vector>
#include <set>
#include <limits>
#include "mwcspreprocessrootrule.h"

namespace nina {
namespace mwcs {

template<typename GR,
         typename WGHT = typename GR::template NodeMap<double> >
class MwcsPreprocessRulePosDeg01 : public MwcsPreprocessRootRule<GR, WGHT>
{
public:
  typedef GR Graph;
  typedef WGHT WeightNodeMap;
  typedef MwcsPreprocessRootRule<GR, WGHT> Parent;
  typedef typename Parent::NodeMap NodeMap;
  typedef typename Parent::NodeSet NodeSet;
  typedef typename Parent::NodeSetIt NodeSetIt;
  typedef typename Parent::NodeSetMap NodeSetMap;
  typedef typename Parent::DegreeNodeMap DegreeNodeMap;
  typedef typename Parent::DegreeNodeSetVector DegreeNodeSetVector;
  typedef typename Parent::LabelNodeMap LabelNodeMap;
  typedef typename Parent::ArcLookUpType ArcLookUpType;

  TEMPLATE_GRAPH_TYPEDEFS(Graph);

  using Parent::remove;
  using Parent::merge;

  MwcsPreprocessRulePosDeg01();
  virtual ~MwcsPreprocessRulePosDeg01() {}
  virtual int apply(Graph& g,
                    Node& root,
                    const ArcLookUpType& arcLookUp,
                    LabelNodeMap& label,
                    WeightNodeMap& score,
                    NodeMap& mapToPre,
                    NodeSetMap& preOrigNodes,
                    int& nNodes,
                    int& nArcs,
                    int& nEdges,
                    DegreeNodeMap& degree,
                    DegreeNodeSetVector& degreeVector);

  virtual std::string name() const { return "Root - PosDeg01"; }
};

template<typename GR, typename WGHT>
inline MwcsPreprocessRulePosDeg01<GR, WGHT>::MwcsPreprocessRulePosDeg01()
  : Parent()
{
}

template<typename GR, typename WGHT>
inline int MwcsPreprocessRulePosDeg01<GR, WGHT>::apply(Graph& g,
                                                       Node& root,
                                                       const ArcLookUpType& arcLookUp,
                                                       LabelNodeMap& label,
                                                       WeightNodeMap& score,
                                                       NodeMap& mapToPre,
                                                       NodeSetMap& preOrigNodes,
                                                       int& nNodes,
                                                       int& nArcs,
                                                       int& nEdges,
                                                       DegreeNodeMap& degree,
                                                       DegreeNodeSetVector& degreeVector)
{
  // determine the max weight nodes among those with deg 0 and 1
  NodeSet maxNodes;
  double maxWeight = -std::numeric_limits<double>::max();

  for (int i = 0; i < 2; i++)
  {
    if (static_cast<int>(degreeVector.size()) <= i)
      continue;

    for (NodeSetIt nodeIt = degreeVector[i].begin(); nodeIt != degreeVector[i].end(); nodeIt++)
    {
      if (score[*nodeIt] > maxWeight)
      {
        maxNodes.clear();
        maxNodes.insert(*nodeIt);
        maxWeight = score[*nodeIt];
      }
      else if (score[*nodeIt] == maxWeight)
      {
        maxNodes.insert(*nodeIt);
      }
    }
  }

  // remove positive degree 0 nodes
  int res = 0;

  if (static_cast<int>(degreeVector.size()) <= 0)
    return res;

  for (NodeSetIt nodeIt = degreeVector[0].begin(); nodeIt != degreeVector[0].end();)
  {
    NodeSetIt nextNodeIt = nodeIt;
    nextNodeIt++;

    if (*nodeIt != root)
    {
      remove(g, mapToPre, preOrigNodes,
             nNodes, nArcs, nEdges,
             degree, degreeVector, *nodeIt);
      res++;
    }

    nodeIt = nextNodeIt;
  }

  if (static_cast<int>(degreeVector.size()) <= 1)
    return res;

  for (NodeSetIt nodeIt = degreeVector[1].begin(); nodeIt != degreeVector[1].end();)
  {
    NodeSetIt nextNodeIt = nodeIt;
    nextNodeIt++;

    if (*nodeIt != root)
    {
      Edge e = IncEdgeIt(g, *nodeIt);

      merge(g, arcLookUp, label,
            score, mapToPre, preOrigNodes,
            nNodes, nArcs, nEdges,
            degree, degreeVector, g.u(e), g.v(e));

      return res + 1;
    }

    nodeIt = nextNodeIt;
  }

  return res;
}

} // namespace mwcs
} // namespace nina

#endif // MWCSPREPROCESSRULEPOSDEG01_H