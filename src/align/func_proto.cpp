/**
 * @file func_proto.cpp
 *
 * @author  Guy Maurel
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "align/func_proto.h"

#include "align/span_num_resolve.h"
#include "align/stack.h"
#include "align/tools.h"
#include "log_rules.h"

#include <memory>
#include <vector>

constexpr static auto LCURRENT = LALPROTO;


using namespace uncrustify;


namespace
{

//! How to start a new AlignStack.
struct StackConfig
{
   size_t                span;
   int                   thresh;
   size_t                gap;
   AlignStack::StarStyle star_style;
   AlignStack::StarStyle amp_style;
};


//! What is kept for one (level, brace level).
struct LevelState
{
   std::unique_ptr<AlignStack> names;  //! the function names
   std::unique_ptr<AlignStack> braces; //! the open braces of single-line functions
   LineSkipConfig              budget; //! the line skip budget
};


/**
 * The states per (level, brace level), created on demand. The grid only grows,
 * so that no pending stack is lost.
 *
 * Issue #2771
 * we align token-1 and token-2 if:
 *   token-1->GetLevel() == token-2->GetLevel()
 *   and
 *   token-1->GetBraceLevel() == token-2->GetBraceLevel()
 * we don't check if token-1 and token-2 are in the same block
 */
class LevelGrid
{
public:
   explicit LevelGrid(const LineSkipConfig &budget)
      : m_budget(budget)
   {
   }


   //! Returns the state of the given (level, brace level), creates it if needed.
   LevelState &At(size_t level, size_t brace_level)
   {
      if (level >= m_states.size())
      {
         m_states.resize(level + 1);
      }
      std::vector<LevelState> &row = m_states[level];

      while (brace_level >= row.size())
      {
         row.emplace_back();
         row.back().budget = m_budget;
      }
      return(row[brace_level]);
   }


   //! Calls 'func' with each state, in (level, brace level) order.
   template<typename Func>
   void ForEach(Func func)
   {
      for (auto &row : m_states)
      {
         for (auto &state : row)
         {
            func(state);
         }
      }
   }

private:
   LineSkipConfig                        m_budget;
   std::vector<std::vector<LevelState> > m_states;
};


//! Returns the stack, starts it if needed.
AlignStack &get_stack(std::unique_ptr<AlignStack> &stack, const StackConfig &config)
{
   if (!stack)
   {
      stack = std::make_unique<AlignStack>();
      stack->Start(config.span, config.thresh);
      stack->m_gap        = config.gap;
      stack->m_star_style = config.star_style;
      stack->m_amp_style  = config.amp_style;
   }
   return(*stack);
}


//! Applies the newlines to the stack, if any.
void new_lines(std::unique_ptr<AlignStack> &stack, size_t cnt)
{
   if (stack)
   {
      stack->NewLines(cnt);
   }
}


/**
 * Whether 'pc' follows another function name on its line, at the same level
 * and brace level: aligning both names on the same line would glue the second
 * one to its return type ('int g(); inthh();'). A newline at a deeper level
 * than 'pc', as in a parameter list or in a function body, does not end the
 * line.
 */
bool is_after_func_on_line(Chunk *pc)
{
   for (Chunk *tmp = pc->GetPrev(); tmp->IsNotNullChunk(); tmp = tmp->GetPrev())
   {
      if (  tmp->IsNewline()
         && tmp->GetLevel() <= pc->GetLevel())
      {
         break;
      }

      if (  (  tmp->Is(E_Token::CT_FUNC_PROTO)
            || (  tmp->Is(E_Token::CT_FUNC_DEF)
               && options::align_single_line_func()))
         && tmp->GetLevel() == pc->GetLevel()
         && tmp->GetBraceLevel() == pc->GetBraceLevel())
      {
         return(true);
      }
   }

   return(false);
}


//! Ends the stack, if any.
void end_stack(std::unique_ptr<AlignStack> &stack)
{
   if (stack)
   {
      stack->End();
      stack.reset();
   }
}

} // namespace


void align_func_proto(size_t span)
{
   LOG_FUNC_ENTRY();

   log_rule_B("align_func_proto_thresh");
   const int mythresh = options::align_func_proto_thresh();

   log_rule_B("align_func_proto_gap");
   log_rule_B("align_func_proto_star_style");
   log_rule_B("align_func_proto_amp_style");
   const StackConfig names_config
   {
      span,
      mythresh,
      options::align_func_proto_gap(),
      static_cast<AlignStack::StarStyle>(options::align_func_proto_star_style()),
      static_cast<AlignStack::StarStyle>(options::align_func_proto_amp_style()),
   };

   log_rule_B("align_single_line_brace_gap");
   const StackConfig braces_config
   {
      span,
      mythresh,
      options::align_single_line_brace_gap(),
      AlignStack::SS_IGNORE,
      AlignStack::SS_IGNORE,
   };

   // Line skip configuration (resolve -1 to global value)
   log_rule_B("align_func_proto_span_num_empty_lines");
   log_rule_B("align_func_proto_span_num_pp_lines");
   log_rule_B("align_func_proto_span_num_cmt_lines");
   const LineSkipConfig myskip_cfg = resolve_span_num_config(
      options::align_func_proto_span_num_empty_lines(),
      options::align_func_proto_span_num_pp_lines(),
      options::align_func_proto_span_num_cmt_lines());

   LevelGrid grid(myskip_cfg);
   bool      look_bro = false;

   for (Chunk *pc = Chunk::GetHead(); pc->IsNotNullChunk(); pc = pc->GetNext())
   {
      char copy[1000];
      LOG_FMT(LAS, "%s(%d): orig line %zu, orig col %zu, text '%s', type %s, level %zu, brace level %zu\n",
              __func__, __LINE__, pc->GetOrigLine(), pc->GetOrigCol(), pc->ElidedText(copy),
              get_token_name(pc->GetType()), pc->GetLevel(), pc->GetBraceLevel());

      LevelState &state = grid.At(pc->GetLevel(), pc->GetBraceLevel());

      if (  pc->IsNewline()
         && (  !options::align_func_proto_span_ignore_cont_lines()
            || !pc->GetNextNnl()->TestFlags(PCF_CONT_LINE))  // Issue #4131
         && (  !pc->TestFlags(PCF_IN_FCN_CALL)               // Issue #2831
            || pc->GetLevel() <= pc->GetBraceLevel()))       // not in the parenthesis
      {
         look_bro = false;

         // Get filtered newline count once for current level/brace_level
         // This modifies the budget which is shared across all stacks at this level
         const size_t nl_cnt = pc->GetNlCountFiltered(state.budget);

         // Apply (filtered) number of newlines, if any left, to all existing stacks
         if (nl_cnt > 0)
         {
            grid.ForEach([nl_cnt](LevelState &st)
            {
               new_lines(st.names, nl_cnt);
            });
         }
         new_lines(state.braces, nl_cnt);
      }
      else if (  (  pc->Is(E_Token::CT_FUNC_PROTO)
                 || (  pc->Is(E_Token::CT_FUNC_DEF)
                    && options::align_single_line_func()))
              && !is_after_func_on_line(pc))
      {
         log_rule_B("align_single_line_func");
         log_rule_B("align_on_operator");
         Chunk *toadd = pc;

         if (  pc->GetParentType() == E_Token::CT_OPERATOR
            && options::align_on_operator())
         {
            toadd = pc->GetPrevNcNnl();
         }
         Chunk *tmp = step_back_over_member(toadd);
         LOG_FMT(LAS, "%s(%d): 'tmp' text is '%s', orig line is %zu, orig col is %zu, level is %zu, brace level is %zu\n",
                 __func__, __LINE__, tmp->GetLogText(), tmp->GetOrigLine(), tmp->GetOrigCol(),
                 tmp->GetLevel(), tmp->GetBraceLevel());
         AlignStack &stack = get_stack(state.names, names_config);
         stack.Add(tmp);
         stack.Debug();
         // Reset budget for next prototype transition at this (level, brace_level)
         state.budget = myskip_cfg;
         log_rule_B("align_single_line_brace");
         look_bro = pc->Is(E_Token::CT_FUNC_DEF)
                    && options::align_single_line_brace();
      }
      else if (  look_bro
              && pc->Is(E_Token::CT_BRACE_OPEN)
              && pc->TestFlags(PCF_ONE_LINER))
      {
         AlignStack &stack = get_stack(state.braces, braces_config);
         stack.Add(pc);
         stack.Debug();
         look_bro = false;
      }
   }

   LOG_FMT(LAS, "%s(%d):  as\n", __func__, __LINE__);

   // purge
   grid.ForEach([](LevelState &st)
   {
      end_stack(st.names);
      end_stack(st.braces);
   });
} // align_func_proto
