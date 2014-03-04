/*
 * heuristicrooted.h
 *
 *  Created on: 27-feb-2014
 *      Author: M. El-Kebir
 */

#ifndef HEURISTICROOTED_H
#define HEURISTICROOTED_H

#include <ilcplex/ilocplex.h>
#include <ilcplex/ilocplex.h>
#include <ilconcert/ilothread.h>
#include <lemon/adaptors.h>
#include <lemon/bfs.h>
#include <set>
#include "../mwcstreesolver.h"

namespace nina {
namespace mwcs {

template<typename GR,
         typename NWGHT = typename GR::template NodeMap<double>,
         typename NLBL = typename GR::template NodeMap<std::string>,
         typename EWGHT = typename GR::template EdgeMap<double> >
class HeuristicRooted : public IloCplex::HeuristicCallbackI
{
public:
  typedef GR Graph;
  typedef NWGHT WeightNodeMap;
  typedef NLBL LabelNodeMap;
  typedef EWGHT WeightEdgeMap;
  
protected:
  TEMPLATE_GRAPH_TYPEDEFS(Graph);
  typedef lemon::FilterEdges<const Graph, const BoolEdgeMap> SubGraphType;
  typedef MwcsGraph<const SubGraphType, const WeightNodeMap, LabelNodeMap, DoubleEdgeMap> MwcsSubGraphType;
  typedef MwcsTreeSolver<const SubGraphType, const WeightNodeMap, LabelNodeMap, DoubleEdgeMap> MwcsSubTreeSolver;
  typedef typename std::set<Node> NodeSet;
  typedef typename NodeSet::const_iterator NodeSetIt;
  
public:
  HeuristicRooted(IloEnv env,
                  IloBoolVarArray x,
                  const Graph& g,
                  const WeightNodeMap& weight,
                  Node root,
                  const IntNodeMap& nodeMap,
                  int n,
                  int m,
                  IloFastMutex* pMutex)
    : IloCplex::HeuristicCallbackI(env)
    , _x(x)
    , _g(g)
    , _weight(weight)
    , _root(root)
    , _nodeMap(nodeMap)
    , _n(n)
    , _m(m)
    , _pEdgeCost(NULL)
    , _pEdgeFilterMap(NULL)
    , _pSubG(NULL)
    , _pMwcsSubGraph(NULL)
    , _pMwcsSubTreeSolver(NULL)
    , _pMutex(pMutex)
  {
    lock();
    _pEdgeCost = new DoubleEdgeMap(_g);
    _pEdgeFilterMap = new BoolEdgeMap(_g, false);
    _pSubG = new SubGraphType(_g, *_pEdgeFilterMap);
    _pMwcsSubGraph = new MwcsSubGraphType();
    _pMwcsSubGraph->init(_pSubG, NULL, &_weight, NULL);
    _pMwcsSubTreeSolver = new MwcsSubTreeSolver(*_pMwcsSubGraph);
    unlock();
  }
  
  HeuristicRooted(const HeuristicRooted& other)
    : IloCplex::HeuristicCallbackI(other._env)
    , _x(other._x)
    , _g(other._g)
    , _weight(other._weight)
    , _root(other._root)
    , _nodeMap(other._nodeMap)
    , _n(other._n)
    , _m(other._m)
    , _pEdgeCost(NULL)
    , _pEdgeFilterMap(NULL)
    , _pSubG(NULL)
    , _pMwcsSubGraph(NULL)
    , _pMwcsSubTreeSolver(NULL)
    , _pMutex(other._pMutex)
  {
    lock();
    _pEdgeCost = new DoubleEdgeMap(_g);
    _pEdgeFilterMap = new BoolEdgeMap(_g, false);
    _pSubG = new SubGraphType(_g, *_pEdgeFilterMap);
    _pMwcsSubGraph = new MwcsSubGraphType();
    _pMwcsSubGraph->init(_pSubG, NULL, &_weight, NULL);
    _pMwcsSubTreeSolver = new MwcsSubTreeSolver(*_pMwcsSubGraph);
    unlock();
  }
  
  ~HeuristicRooted()
  {
    lock();
    delete _pMwcsSubTreeSolver;
    delete _pMwcsSubGraph;
    delete _pEdgeCost;
    delete _pEdgeFilterMap;
    delete _pSubG;
    unlock();
  }
  
protected:
  virtual void main()
  {
    computeEdgeWeights();
    computeMinimumCostSpanningTree();
    
    IloBoolVarArray solutionVar(_env, 0);
    IloNumArray solution(_env, 0);
    double solutionWeight = hasIncumbent() ? getIncumbentObjValue() : -1;
    
    if (computeMaxWeightConnectedSubtree(solutionVar, solution, solutionWeight))
    {
      setCplexSolution(solutionVar, solution, solutionWeight);
    }
    
    solutionVar.end();
    solution.end();
  }

  virtual IloCplex::CallbackI* duplicateCallback() const
  {
    return (new (_env) HeuristicRooted(*this));
  }
  
  void lock()
  {
    if (_pMutex)
      _pMutex->lock();
  }
  
  void unlock()
  {
    if (_pMutex)
      _pMutex->unlock();
  }
  
  void setCplexSolution(IloBoolVarArray solutionVar, IloNumArray solution, double solutionWeight)
  {
    setSolution(solutionVar, solution, solutionWeight);
  }
  
  void computeEdgeWeights()
  {
    IloNumArray x_values(_env, _n);
    getValues(x_values, _x);
    
    for (EdgeIt e(_g); e != lemon::INVALID; ++e)
    {
      Node u = _g.u(e);
      Node v = _g.v(e);
      
      double x_u = x_values[_nodeMap[u]];
      double x_v = x_values[_nodeMap[v]];
      
      _pEdgeCost->set(e, 2 - (x_u + x_v));
    }
    
    x_values.end();
  }
  
  void computeMinimumCostSpanningTree()
  {
    lock();
    lemon::kruskal(_g, *_pEdgeCost, *_pEdgeFilterMap);
    unlock();
  }
  
  bool computeMaxWeightConnectedSubtree(IloBoolVarArray& solutionVar,
                                        IloNumArray& solution,
                                        double& solutionWeight)
  {
    _pMwcsSubTreeSolver->init(_root);
    _pMwcsSubTreeSolver->solve();
    
    if (_pMwcsSubTreeSolver->getSolutionWeight() > solutionWeight)
    {
      solutionWeight = _pMwcsSubTreeSolver->getSolutionWeight();
      solutionVar.add(_x);
      solution.add(_x.getSize(), 0);
      
      const NodeSet& module = _pMwcsSubTreeSolver->getSolutionModule();
      for (NodeSetIt it = module.begin(); it != module.end(); ++it)
      {
        solution[_nodeMap[*it]] = 1;
      }
      
      return true;
    }
    
    return false;
  }
  
protected:
  IloBoolVarArray _x;
  const Graph& _g;
  const WeightNodeMap& _weight;
  Node _root;
  const IntNodeMap& _nodeMap;
  const int _n;
  const int _m;
  DoubleEdgeMap* _pEdgeCost;
  BoolEdgeMap* _pEdgeFilterMap;
  const SubGraphType* _pSubG;
  MwcsSubGraphType* _pMwcsSubGraph;
  MwcsSubTreeSolver* _pMwcsSubTreeSolver;
  IloFastMutex* _pMutex;
};
  
} // namespace mwcs
} // namespace nina

#endif // HEURISTICROOTED_H