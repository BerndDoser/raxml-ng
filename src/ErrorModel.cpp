#include <assert.h>

#include "ErrorModel.hpp"

#define HOMO(state)   (state<4)
#define HETERO(state) (state>3)

/*                                        AA CC GG TT AC AG AT CG CT GT            */
static const char mut_dist[10][10] = {  {  0, 2, 2, 2, 1, 1, 1, 2, 2, 2 },   /* AA */
                                        {  2, 0, 2, 2, 1, 2, 2, 1, 1, 2 },   /* CC */
                                        {  2, 2, 0, 2, 2, 1, 2, 1, 2, 1 },   /* GG */
                                        {  2, 2, 2, 0, 2, 2, 1, 2, 1, 1 },   /* TT */
                                        {  1, 1, 2, 2, 0, 1, 1, 1, 1, 2 },   /* AC */
                                        {  1, 2, 1, 2, 1, 0, 1, 1, 2, 1 },   /* AG */
                                        {  1, 2, 2, 1, 1, 1, 0, 2, 1, 1 },   /* AT */
                                        {  2, 1, 1, 2, 1, 1, 2, 0, 1, 1 },   /* CG */
                                        {  2, 1, 2, 1, 1, 2, 1, 1, 0, 1 },   /* CT */
                                        {  2, 2, 1, 1, 2, 1, 1, 1, 1, 0 }    /* GT */
                                 };

LogStream& operator<<(LogStream& stream, const ErrorModel& m)
{
  stream << m.name();

  // print model parameters
  auto names = m.param_names();
  auto values = m.params();
  assert(names.size() == values.size());

  for (size_t i = 0; i < names.size(); ++i)
  {
    stream << ",  " << names[i] << ": " << values[i];
  }

  return stream;
}

intVector UniformErrorModel::param_ids() const
{
  intVector ids;
  ids.push_back(RAXML_OPT_PARAM_SEQ_ERROR);
  return ids;
}

NameVector UniformErrorModel::param_names() const
{
  NameVector names;
  names.push_back("SEQ_ERROR");
  return names;
}

doubleVector UniformErrorModel::params() const
{
  doubleVector params;
  params.push_back(_seq_error_rate);
  return params;
}

void UniformErrorModel::params(const doubleVector& pv)
{
  assert(pv.size() >= 1);
  _seq_error_rate = pv[0];
}

void UniformErrorModel::compute_state_probs(unsigned int state,
                                            doubleVector::iterator &clvp) const
{
  unsigned int state_id = __builtin_ctz(state);
  unsigned int bitset   = __builtin_popcount(state);
  unsigned int bitunset = _states - bitset;

  // TODO: move it out of here
  unsigned int undef_state = (unsigned int) (pow(2, _states)) - 1;

  for (size_t k = 0; k < _states; ++k)
  {
    if (state == undef_state)
      clvp[k] = 1.;
    else
    {
      if (k == state_id)
      {
        clvp[k] = (1. - _seq_error_rate) / bitset;
      }
      else
      {
        clvp[k] = _seq_error_rate / bitunset;
      }
    }
  }
}

intVector GenotypeErrorModel::param_ids() const
{
  intVector ids;
  ids.push_back(RAXML_OPT_PARAM_SEQ_ERROR);
  ids.push_back(RAXML_OPT_PARAM_ADO_RATE);
  return ids;
}

NameVector GenotypeErrorModel::param_names() const
{
  NameVector names;
  names.push_back("SEQ_ERROR");
  names.push_back("ADO_RATE");
  return names;
}

doubleVector GenotypeErrorModel::params() const
{
  doubleVector params;
  params.push_back(_seq_error_rate);
  params.push_back(_dropout_rate);
  return params;
}

void GenotypeErrorModel::params(const doubleVector& pv)
{
  assert(pv.size() > 0);

  _seq_error_rate = pv[0];

  if (pv.size() > 1)
    _dropout_rate = pv[1];
}

void P17GenotypeErrorModel::compute_state_probs(unsigned int state,
                                                doubleVector::iterator &clvp) const
{
  unsigned int state_id = __builtin_ctz(state);
  static const double one_3 = 1. / 3.;
  static const double one_6 = 1. / 6.;

  // TODO: move it out of here
  unsigned int undef_state = (unsigned int) (pow(2, _states)) - 1;

  double sum_lh = 0.;

  for (size_t k = 0; k < _states; ++k)
  {
    if (state == undef_state)
      clvp[k] = 1.;
    else
    {
      if (k == state_id)
      {
        if (HOMO(state_id))
          clvp[k] = 1. - _seq_error_rate + 0.5 * _seq_error_rate * _dropout_rate;
        else
          clvp[k] = 1. - _seq_error_rate - _dropout_rate + _seq_error_rate * _dropout_rate;
      }
      else if (mut_dist[state_id][k] == 1)
      {
        if (HOMO(k))
          clvp[k] = (1. - _dropout_rate) * _seq_error_rate * one_3;
        else
        {
          if (HOMO(state_id))
            clvp[k] = 0.5 * _dropout_rate + one_6 * _seq_error_rate -
                one_3 * _seq_error_rate * _dropout_rate;
          else
            clvp[k] = (1. - _dropout_rate) * _seq_error_rate * one_6;
        }
      }
      else if (HOMO(state_id))
        clvp[k] = one_6 * _seq_error_rate * _dropout_rate;
      else
        clvp[k] = 0.;

      sum_lh += clvp[k];
    }
  }

//  printf("sumlh: %lf\n", sum_lh);
//  if (state != undef_state)
//  {
//    for (size_t k = 0; k < _states; ++k)
//      clvp[k] /= sum_lh;
//  }
}

void PT19GenotypeErrorModel::compute_state_probs(unsigned int state,
                                                doubleVector::iterator &clvp) const
{
  unsigned int state_id = __builtin_ctz(state);
  static const double one_3 = 1. / 3.;
  static const double one_6 = 1. / 6.;
  static const double one_8 = 1. / 8.;
  static const double three_8 = 3. / 8.;
  static const double one_12 = 1. / 12.;

  // TODO: move it out of here
  unsigned int undef_state = (unsigned int) (pow(2, _states)) - 1;

  double sum_lh = 0.;

  for (size_t k = 0; k < _states; ++k)
  {
    if (state == undef_state)
      clvp[k] = 1.;
    else
    {
      if (k == state_id)
      {
        /* 0 letters away */
        if (HOMO(state_id))
          clvp[k] = 1. - _seq_error_rate + 0.5 * _seq_error_rate * _dropout_rate;
        else
          clvp[k] =  (1. - _dropout_rate ) * (1. - _seq_error_rate) + one_12 * _seq_error_rate * _dropout_rate;
      }
      else if (mut_dist[state_id][k] == 1)
      {
        /* 1 letter away */
        if (HOMO(k))
        {
          clvp[k] = one_12 * _seq_error_rate * _dropout_rate +
                    one_3  * (1. - _dropout_rate) * _seq_error_rate;
        }
        else
        {
          if (HOMO(state_id))
          {
            clvp[k] = 0.5 * _dropout_rate + one_6 * _seq_error_rate -
                      three_8 * _seq_error_rate * _dropout_rate;
          }
          else
          {
            clvp[k] = one_6 * _seq_error_rate -
                      one_8 * _seq_error_rate * _dropout_rate;
          }
        }
      }
      else
      {
        /* 2 letters away */
        if (HOMO(state_id))
          clvp[k] = one_12 * _seq_error_rate * _dropout_rate;
        else
          clvp[k] = 0.;
      }

      sum_lh += clvp[k];
    }
  }
}


