#include "thread_vision_utils.h"

void suppress_tangents(vector<tangent_and_score>& tangents, vector<tangent_and_score>& tangents_to_keep)
{
  vector<int> inds_to_keep;
  suppress_tangents(tangents, inds_to_keep);

  tangents_to_keep.resize(0);
  for (int i=0; i < inds_to_keep.size(); i++)
  {
    tangents_to_keep.push_back(tangents[inds_to_keep[i]]);
  }
}

void suppress_tangents(vector<tangent_and_score>& tangents, vector<int>& inds_to_keep)
{
  //const double dot_prod_thresh = 0.1;
  const double dot_prod_thresh = 0.2;
  const double position_norm_thresh = 2.0;
  const double total_score_thresh = 4.0;
  inds_to_keep.resize(0);

  sort(tangents.begin(), tangents.end());
  int ind_checking;
  for (ind_checking = 0; ind_checking < tangents.size(); ind_checking++)
  {
    if (inds_to_keep.size() > 0 && tangents[ind_checking].score > tangents.front().score * total_score_thresh)
      break;

    bool keep_this_ind = true;
    for (int ind_comparing = 0; ind_comparing < inds_to_keep.size(); ind_comparing++)
    {
      double dot_prod = tangents[ind_checking].tan.dot(tangents[inds_to_keep[ind_comparing]].tan);
      if (dot_prod > dot_prod_thresh)
      {
        keep_this_ind = false;
      }
    }
    if (keep_this_ind)
      inds_to_keep.push_back(ind_checking);
  }

}


bool operator <(const tangent_and_score& a, const tangent_and_score& b)
{
  return a.score < b.score;
}

