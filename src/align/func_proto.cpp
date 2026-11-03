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
   bool                  ref_prev;
};


//! What is aligned during one pass over the chunks.
struct ProtoPass
{
   bool names;  //! the function names
   bool parens; //! the open parenthesis (align_func_proto_paren)
   bool braces; //! the open braces of single-line functions
};

//! The passes over the chunks.
constexpr ProtoPass NAMES_AND_BRACES = { true, false, true };
constexpr ProtoPass NAMES_ONLY       = { true, false, false };
constexpr ProtoPass PARENS_ONLY      = { false, true, false };
constexpr ProtoPass BRACES_ONLY      = { false, false, true };


/**
 * What is kept for one (level, brace level).
 *
 * The constructors and destructors have their own groups, with their own
 * line skip budget: they have no return type, so their names are not
 * aligned and they don't belong to the groups of the other functions.
 */
struct LevelState
{
   std::unique_ptr<AlignStack> names;       //! the function names
   std::unique_ptr<AlignStack> parens;      //! the open parenthesis of the functions
   std::unique_ptr<AlignStack> braces;      //! the open braces of single-line functions
   std::unique_ptr<AlignStack> ctor_parens; //! the open parenthesis of the constructors
   std::unique_ptr<AlignStack> ctor_braces; //! the open braces of single-line constructors
   LineSkipConfig              budget;      //! the line skip budget of the functions
   LineSkipConfig              ctor_budget; //! the line skip budget of the constructors
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
         row.back().budget      = m_budget;
         row.back().ctor_budget = m_budget;
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
      stack->m_ref_prev   = config.ref_prev;
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


//! Whether there is no newline from 'first' to 'last', comments included.
bool is_on_one_line(Chunk *first, Chunk *last)
{
   for (Chunk *tmp = first; tmp->IsNotNullChunk(); tmp = tmp->GetNext())
   {
      if (  tmp->IsNewline()
         || tmp->GetNlCount() > 0)
      {
         return(false);
      }

      if (tmp == last)
      {
         return(true);
      }
   }

   return(false);
}


/**
 * Returns the open parenthesis following the function name 'func', after its
 * template arguments if any, or the null chunk if there is none.
 */
Chunk *get_func_paren(Chunk *func)
{
   Chunk *paren = func->GetNextNc();

   // skip the template arguments, i.e. 'foo<T>('
   if (paren->Is(E_Token::CT_ANGLE_OPEN))
   {
      paren = paren->GetClosingParen()->GetNextNc();
   }

   if (  paren->IsNot(E_Token::CT_FPAREN_OPEN)
      || paren->GetLevel() != func->GetLevel())
   {
      return(Chunk::NullChunkPtr);
   }
   return(paren);
}


/**
 * Whether 'name' is a call of a constructor, not its declaration, which the
 * tokenizer can also mark as CT_FUNC_CLASS_PROTO or CT_FUNC_CLASS_DEF: a
 * delegating constructor or base class call ('B()' in 'B(int a) : B() {}'),
 * a default member initializer ('Point origin = Point(0);') or a 'new'
 * expression.
 */
bool is_ctor_call(Chunk *name)
{
   const Chunk *prev = name->GetPrevNcNnl();

   return(  prev->Is(E_Token::CT_CONSTR_COLON)
         || prev->Is(E_Token::CT_COMMA)
         || prev->Is(E_Token::CT_ASSIGN)
         || prev->Is(E_Token::CT_ASSIGN_DEFAULT_ARG)
         || prev->Is(E_Token::CT_NEW)
         || prev->IsParenOpen());
}


//! Whether 'pc' is the name of a constructor or destructor prototype or definition.
bool is_ctor_name(Chunk *pc)
{
   if (pc->Is(E_Token::CT_FUNC_CLASS_PROTO))
   {
      return(!is_ctor_call(pc));
   }

   if (pc->IsNot(E_Token::CT_FUNC_CLASS_DEF))
   {
      return(false);
   }
   log_rule_B("align_single_line_func");
   return(  options::align_single_line_func()
         && !is_ctor_call(pc));
}


//! Whether 'pc' is the name of a function aligned by align_func_proto.
bool is_func_name(Chunk *pc)
{
   if (pc->Is(E_Token::CT_FUNC_PROTO))
   {
      return(true);
   }

   if (pc->IsNot(E_Token::CT_FUNC_DEF))
   {
      return(false);
   }
   log_rule_B("align_single_line_func");
   return(options::align_single_line_func());
}


/**
 * Whether 'pc' follows another function name on its line, at the same level
 * and brace level: aligning both names on the same line would glue the second
 * one to its return type ('int g(); inthh();'). Only the first function of a
 * line is aligned. With align_func_proto_paren=true, the constructors and
 * destructors count too. A newline at a deeper level than 'pc', as in a
 * parameter list or in a function body, does not end the line.
 */
bool is_after_func_on_line(Chunk *pc)
{
   log_rule_B("align_func_proto_paren");
   const bool with_ctors = options::align_func_proto_paren();

   for (Chunk *tmp = pc->GetPrev(); tmp->IsNotNullChunk(); tmp = tmp->GetPrev())
   {
      if (  tmp->IsNewline()
         && tmp->GetLevel() <= pc->GetLevel())
      {
         break;
      }

      if (  tmp->GetLevel() == pc->GetLevel()
         && tmp->GetBraceLevel() == pc->GetBraceLevel()
         && (  is_func_name(tmp)
            || (  with_ctors
               && is_ctor_name(tmp))))
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


/**
 * Does one pass over the chunks.
 *
 * When align_func_proto_paren=true, the names, the open parenthesis and the
 * open braces are aligned in separate passes: aligning the names moves the
 * parenthesis and aligning the parenthesis moves the braces, so each kind of
 * stack is flushed only when the columns of the previous kind are final.
 */
void align_func_proto_pass(size_t span, const ProtoPass &pass)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LAS, "%s(%d): pass: names %d, parens %d, braces %d\n",
           __func__, __LINE__, pass.names, pass.parens, pass.braces);

   // The constructors and destructors are only considered when their
   // open parenthesis are aligned.
   log_rule_B("align_func_proto_paren");
   const bool with_ctors = options::align_func_proto_paren();

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
      false,
   };

   log_rule_B("align_func_proto_paren_gap");
   const StackConfig parens_config
   {
      span,
      mythresh,
      options::align_func_proto_paren_gap(),
      AlignStack::SS_IGNORE,
      AlignStack::SS_IGNORE,
      // the open parenthesis follows the name: an operator name such as '*'
      // or '&' must not be taken for a pointer operator
      true,
   };

   log_rule_B("align_single_line_brace_gap");
   const StackConfig braces_config
   {
      span,
      mythresh,
      options::align_single_line_brace_gap(),
      AlignStack::SS_IGNORE,
      AlignStack::SS_IGNORE,
      false,
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
   bool      look_bro      = false;
   bool      look_ctor_bro = false;

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
         look_bro      = false;
         look_ctor_bro = false;

         // Get filtered newline count once for current level/brace_level
         // This modifies the budget which is shared across all stacks at this level
         const size_t nl_cnt = pc->GetNlCountFiltered(state.budget);

         // Apply (filtered) number of newlines, if any left, to all existing stacks
         if (nl_cnt > 0)
         {
            grid.ForEach([nl_cnt](LevelState &st)
            {
               new_lines(st.names, nl_cnt);
               new_lines(st.parens, nl_cnt);
            });
         }
         new_lines(state.braces, nl_cnt);

         if (with_ctors)
         {
            const size_t ctor_nl_cnt = pc->GetNlCountFiltered(state.ctor_budget);

            if (ctor_nl_cnt > 0)
            {
               grid.ForEach([ctor_nl_cnt](LevelState &st)
               {
                  new_lines(st.ctor_parens, ctor_nl_cnt);
               });
            }
            new_lines(state.ctor_braces, ctor_nl_cnt);
         }
      }
      else if (  (  pc->Is(E_Token::CT_FUNC_PROTO)
                 || (  pc->Is(E_Token::CT_FUNC_DEF)
                    && options::align_single_line_func()))
              && !is_after_func_on_line(pc))
      {
         log_rule_B("align_single_line_func");

         if (pass.names)
         {
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
         }
         Chunk *paren = pass.parens ? get_func_paren(pc) : Chunk::NullChunkPtr;

         if (  paren->IsNotNullChunk()
            && func_proto_paren_is_aligned(paren))
         {
            AlignStack &stack = get_stack(state.parens, parens_config);
            stack.Add(paren);
            stack.Debug();
         }
         // Reset budget for next prototype transition at this (level, brace_level)
         state.budget = myskip_cfg;
         log_rule_B("align_single_line_brace");
         look_bro = pass.braces
                    && pc->Is(E_Token::CT_FUNC_DEF)
                    && options::align_single_line_brace();
      }
      else if (  with_ctors
              && is_ctor_name(pc)
              && !is_after_func_on_line(pc))
      {
         // Constructors and destructors: only the open parenthesis is aligned
         Chunk *paren = pass.parens ? get_func_paren(pc) : Chunk::NullChunkPtr;

         if (  paren->IsNotNullChunk()
            && func_proto_paren_is_aligned(paren))
         {
            AlignStack &stack = get_stack(state.ctor_parens, parens_config);
            stack.Add(paren);
            stack.Debug();
         }
         // Reset budget for next constructor transition at this (level, brace_level)
         state.ctor_budget = myskip_cfg;
         log_rule_B("align_single_line_brace");
         look_ctor_bro = pass.braces
                         && pc->Is(E_Token::CT_FUNC_CLASS_DEF)
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
      else if (  look_ctor_bro
              && pc->Is(E_Token::CT_BRACE_OPEN)
              && pc->TestFlags(PCF_ONE_LINER))
      {
         AlignStack &stack = get_stack(state.ctor_braces, braces_config);
         stack.Add(pc);
         stack.Debug();
         look_ctor_bro = false;
      }
   }

   LOG_FMT(LAS, "%s(%d):  as\n", __func__, __LINE__);

   // purge
   grid.ForEach([](LevelState &st)
   {
      end_stack(st.names);
      end_stack(st.parens);
      end_stack(st.braces);
      end_stack(st.ctor_parens);
      end_stack(st.ctor_braces);
   });
} // align_func_proto_pass

} // namespace


bool func_proto_paren_is_aligned(Chunk *paren)
{
   if (paren->IsNot(E_Token::CT_FPAREN_OPEN))
   {
      return(false);
   }
   const E_Token parent = paren->GetParentType();

   if (  parent != E_Token::CT_FUNC_PROTO
      && parent != E_Token::CT_FUNC_DEF
      && parent != E_Token::CT_FUNC_CLASS_PROTO
      && parent != E_Token::CT_FUNC_CLASS_DEF)
   {
      return(false);
   }
   log_rule_B("align_func_proto_paren");
   log_rule_B("align_func_proto_span");
   log_rule_B("align_mix_var_proto");

   if (  !options::align_func_proto_paren()
      || options::align_func_proto_span() == 0
      || options::align_mix_var_proto())
   {
      return(false);
   }
   // the function name, before the template arguments if any
   Chunk *name = paren->GetPrevNc();

   if (name->Is(E_Token::CT_ANGLE_CLOSE))
   {
      name = name->GetOpeningParen()->GetPrevNc();
   }

   if (  !(  is_func_name(name)
          || is_ctor_name(name))
      || !is_on_one_line(name, paren)
      || is_after_func_on_line(name))
   {
      return(false);
   }
   // A second parameter list, as in the D template 'T get(T)(int i);', means
   // that this one is not the function parameter list.
   return(!paren->GetClosingParen()->GetNextNc()->IsParenOpen());
} // func_proto_paren_is_aligned


void align_func_proto(size_t span)
{
   LOG_FUNC_ENTRY();

   log_rule_B("align_func_proto_paren");

   if (!options::align_func_proto_paren())
   {
      align_func_proto_pass(span, NAMES_AND_BRACES);
      return;
   }
   align_func_proto_pass(span, NAMES_ONLY);
   align_func_proto_pass(span, PARENS_ONLY);

   log_rule_B("align_single_line_func");
   log_rule_B("align_single_line_brace");

   if (  options::align_single_line_func()
      && options::align_single_line_brace())
   {
      align_func_proto_pass(span, BRACES_ONLY);
   }
} // align_func_proto
