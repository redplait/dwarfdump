std::map<unsigned short, std::set<unsigned short> > g_graph;

void read_graph()
{
  for ( size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i )
  {
    g_graph[data[i].e1].insert(data[i].e2);
    g_graph[data[i].e2].insert(data[i].e1);
  }
}

int is_clique(std::set<unsigned short> &v)
{
  std::vector<unsigned short> varr;
  std::copy(v.begin(), v.end(), std::back_inserter(varr));
  for ( size_t i = 0; i < varr.size(); ++i )
    for ( size_t j = i + 1; j < varr.size(); ++j )
    {
      auto vert = g_graph.find(varr[i]);
      if ( vert == g_graph.end() )
      {
        // printf("cannot find vertex %d\n", varr[i]);
        return 0;
      }
      auto f = vert->second.find(varr[j]);
      if ( f == vert->second.end() )
        return 0;
    }
  return 1;
}
