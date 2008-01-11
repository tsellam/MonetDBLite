/**
 * @file
 *
 * This property phase follows a list of columns starting from a start
 * operator until a specified goal operator is reached.
 *
 * Copyright Notice:
 * -----------------
 *
 * The contents of this file are subject to the Pathfinder Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://monetdb.cwi.nl/Legal/PathfinderLicense-1.1.html
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Pathfinder system.
 *
 * The Original Code has initially been developed by the Database &
 * Information Systems Group at the University of Konstanz, Germany and
 * is now maintained by the Database Systems Group at the Technische
 * Universitaet Muenchen, Germany.  Portions created by the University of
 * Konstanz and the Technische Universitaet Muenchen are Copyright (C)
 * 2000-2005 University of Konstanz and (C) 2005-2008 Technische
 * Universitaet Muenchen, respectively.  All Rights Reserved.
 *
 * $Id$
 */

/* always include pathfinder.h first! */
#include "pathfinder.h"
#include <assert.h>
#include <stdio.h>

#include "properties.h"
#include "alg_dag.h"
#include "oops.h"
#include "mem.h"

/*
 * Easily access subtree-parts.
 */
/** starting from p, make a step left */
#define L(p) ((p)->child[0])
/** starting from p, make a step right */
#define R(p) ((p)->child[1])
/** starting from p, make a step right, then a step left */
#define RL(p) L(R(p))
/** starting from p, make two steps right */
#define RR(p) R(R(p))

/* store the number of incoming edges for each operator
   in the state_label field */
#define EDGE(n) ((n)->state_label)

#define CUR_AT(n,i) (((name_pair_t *) PFarray_at ((n), (i)))->unq)
#define ORI_AT(n,i) (((name_pair_t *) PFarray_at ((n), (i)))->ori)

/**
 * Add a new original name/current name pair to the list of name pairs @a np
 */
static void
add_name_pair (PFarray_t *np, PFalg_att_t ori, PFalg_att_t cur)
{
    assert (np);

    *(name_pair_t *) PFarray_add (np)
        = (name_pair_t) { .ori = ori, .unq = cur };
}

/**
 * diff_np marks the name pair invalid that is associated
 * with the current attribute name @a cur.
 */
static void
diff_np (PFarray_t *np_list, PFalg_att_t cur)
{
    for (unsigned int i = 0; i < PFarray_last (np_list); i++)
        if (CUR_AT(np_list, i) == cur) {
            /* mark the name pair as invalid */
            CUR_AT(np_list, i) = att_NULL;
            break;
        }
}

/**
 * Worker for PFprop_trace_names(). It recursively propagates
 * the name pair list.
 */
static void
map_names (PFla_op_t *n, PFla_op_t *goal, PFarray_t *par_np_list,
           PFalg_att_t twig_iter)
{
    PFalg_att_t cur, ori;
    PFarray_t  *np_list = n->prop->name_pairs;

    assert (n);
    assert (np_list);
    assert (par_np_list);

    /* collect all name pair lists of the parent operators and
       include (possibly) new matching columns in the name pairs list */
    if (!PFarray_last (np_list))
        /* Copy the complete name pair list of the parent
           if we have no name pair list so far. */
        for (unsigned int i = 0; i < PFarray_last (par_np_list); i++)
            add_name_pair (np_list,
                           ORI_AT(par_np_list, i),
                           CUR_AT(par_np_list, i));
    else
        /* Otherwise adjust the name pair list. */
        for (unsigned int i = 0; i < PFarray_last (par_np_list); i++) {
            ori = ORI_AT(par_np_list, i);
            cur = CUR_AT(par_np_list, i);

            /* Use the name pair entry of the parent if the name pair
               is unknown (original name is 0). Name pairs marked
               as unknown are introduced by a projection that prunes
               the column. */
            if (!ORI_AT(np_list,i)) {
                ORI_AT(np_list, i) = ori;
                CUR_AT(np_list, i) = cur;
            }
            /* Mark the name pair as invalid if the name pair of the
               parent is known (original name is not 0) and we have
               conflicting current names. */
            else if (ori && cur != CUR_AT(np_list, i)) {
                CUR_AT(np_list, i) = att_NULL;
            }
        }

    /* nothing to do if we haven't collected
       all incoming name pair lists of that node */
    EDGE(n)++;
    if (EDGE(n) < PFprop_refctr (n))
        return;

    /* If we reached our goal we can return.
       (The resulting name pair list is accessible
        using the reference to operator goal.) */
    if (n == goal)
        return;

    /* Remove all the name pairs from the list whose current column name
       is generated by this operator and map the current name in case
       this operator is a renaming projection. */
    switch (n->kind) {
        case la_serialize_seq:
        case la_serialize_rel:
        case la_lit_tbl:
        case la_empty_tbl:
        case la_ref_tbl:
        case la_select:
        case la_pos_select:
        case la_error:
        case la_distinct:
        case la_disjunion:
        case la_intersect:
        case la_difference:
        case la_count:
        case la_type_assert:
        case la_roots:
        case la_proxy:
        case la_proxy_base:
        case la_dummy:
            break;

        case la_attach:
            diff_np (np_list, n->sem.attach.res);
            break;

        case la_project:
        {
            unsigned int j;
            for (unsigned int i = 0; i < PFarray_last (np_list); i++) { 
                /* Adjust all current column names for the columns
                   in the projection list. */
                for (j = 0; j < n->sem.proj.count; j++)
                    if (n->sem.proj.items[j].new == CUR_AT(np_list, i)) {
                        CUR_AT(np_list, i) = n->sem.proj.items[j].old;
                        break;
                    }
                /* If the name pair is not referenced in the projection
                   list and it is not marked as invalid we mark the
                   name pair as unknown. (Throwing away both column
                   names does not cause trouble as all name pair lists
                   are aligned.) */
                if (j == n->sem.proj.count &&
                    CUR_AT(np_list, i) != att_NULL) {
                    CUR_AT(np_list, i) = att_NULL;
                    ORI_AT(np_list, i) = att_NULL;
                }
            }
        }   break;

        case la_cross:
        case la_eqjoin:
        case la_semijoin:
        case la_thetajoin:
        {
            unsigned int j;
            /* if we have no additional name pair list then create one */
            if (!n->prop->l_name_pairs)
               n->prop->l_name_pairs = PFarray (sizeof (name_pair_t));

            /* mark all columns that we do not see in the left child
               as unknown */
            for (unsigned int i = 0; i < PFarray_last (np_list); i++) {
                for (j = 0; j < L(n)->schema.count; j++)
                    if (L(n)->schema.items[j].name == CUR_AT(np_list, i)) {
                        add_name_pair (n->prop->l_name_pairs,
                                       ORI_AT(np_list, i),
                                       CUR_AT(np_list, i));
                        break;
                    }
                if (j == L(n)->schema.count)
                    add_name_pair (n->prop->l_name_pairs, att_NULL, att_NULL);
            }
            map_names (L(n), goal, n->prop->l_name_pairs, att_NULL);
            PFarray_last (n->prop->l_name_pairs) = 0;
            
            if (n->kind == la_semijoin)
                /* mark all columns in the right child as unknown */
                for (unsigned int i = 0; i < PFarray_last (np_list); i++)
                    add_name_pair (n->prop->l_name_pairs, att_NULL, att_NULL);
            else
                /* mark all columns that we do not see in the right child
                   as unknown */
                for (unsigned int i = 0; i < PFarray_last (np_list); i++) {
                    for (j = 0; j < R(n)->schema.count; j++)
                        if (R(n)->schema.items[j].name == CUR_AT(np_list, i)) {
                            add_name_pair (n->prop->l_name_pairs,
                                           ORI_AT(np_list, i),
                                           CUR_AT(np_list, i));
                            break;
                        }
                    if (j == R(n)->schema.count)
                        add_name_pair (n->prop->l_name_pairs,
                                       att_NULL,
                                       att_NULL);
                }
            map_names (R(n), goal, n->prop->l_name_pairs, att_NULL);
            PFarray_last (n->prop->l_name_pairs) = 0;
        }   return;
        
        case la_fun_1to1:
            diff_np (np_list, n->sem.fun_1to1.res);
            break;

        case la_num_eq:
        case la_num_gt:
        case la_bool_and:
        case la_bool_or:
            diff_np (np_list, n->sem.binary.res);
            break;

        case la_bool_not:
            diff_np (np_list, n->sem.unary.res);
            break;

        case la_to:
            diff_np (np_list, n->sem.to.res);
            break;

        case la_avg:
        case la_max:
        case la_min:
        case la_sum:
        case la_seqty1:
        case la_all:
            diff_np (np_list, n->sem.aggr.att);
            break;

        case la_rownum:
        case la_rowrank:
        case la_rank:
            diff_np (np_list, n->sem.sort.res);
            break;

        case la_rowid:
            diff_np (np_list, n->sem.rowid.res);
            break;

        case la_type:
        case la_cast:
            diff_np (np_list, n->sem.type.res);
            break;

        case la_step:
        case la_step_join:
        case la_guide_step:
        case la_guide_step_join:
            diff_np (np_list, n->sem.step.item_res);
            break;

        case la_doc_index_join:
            diff_np (np_list, n->sem.doc_join.item_res);
            break;

        case la_doc_tbl:
            diff_np (np_list, n->sem.doc_tbl.item_res);
            break;

        case la_doc_access:
            diff_np (np_list, n->sem.doc_access.res);
            break;

        case la_twig:
            diff_np (np_list, n->sem.iter_item.item);
            /* make sure the underlying constructors
               propagate the correct information */
            map_names (L(n), goal, np_list, n->sem.iter_item.iter);
            return;

        case la_fcns:
            map_names (L(n), goal, np_list, twig_iter);
            map_names (R(n), goal, np_list, twig_iter);
            break;

        case la_docnode:
            for (unsigned int i = 0; i < PFarray_last (np_list); i++)
                /* Adjust all current column names for the loop columns. */
                if (twig_iter == CUR_AT(np_list, i))
                    CUR_AT(np_list, i) = n->sem.docnode.iter;
                
            /* infer properties for children and
               return the resulting mapping */
            map_names (L(n), goal, np_list, att_NULL);
            map_names (R(n), goal, np_list, n->sem.docnode.iter);
            return;

        case la_element:
            for (unsigned int i = 0; i < PFarray_last (np_list); i++)
                /* Adjust all current column names for the loop columns. */
                if (twig_iter == CUR_AT(np_list, i))
                    CUR_AT(np_list, i) = n->sem.iter_item.iter;
                
            /* infer properties for children and
               return the resulting mapping */
            map_names (L(n), goal, np_list, att_NULL);
            map_names (R(n), goal, np_list, n->sem.iter_item.iter);
            return;

        case la_textnode:
        case la_comment:
            for (unsigned int i = 0; i < PFarray_last (np_list); i++)
                /* Adjust all current column names for the loop columns. */
                if (twig_iter == CUR_AT(np_list, i))
                    CUR_AT(np_list, i) = n->sem.iter_item.iter;
                
            /* infer properties for children and
               return the resulting mapping */
            map_names (L(n), goal, np_list, att_NULL);
            return;

        case la_attribute:
        case la_processi:
            for (unsigned int i = 0; i < PFarray_last (np_list); i++)
                /* Adjust all current column names for the loop columns. */
                if (twig_iter == CUR_AT(np_list, i))
                    CUR_AT(np_list, i) = n->sem.iter_item1_item2.iter;
                
            /* infer properties for children and
               return the resulting mapping */
            map_names (L(n), goal, np_list, att_NULL);
            return;

        case la_content:
            for (unsigned int i = 0; i < PFarray_last (np_list); i++)
                /* Adjust all current column names for the loop columns. */
                if (twig_iter == CUR_AT(np_list, i))
                    CUR_AT(np_list, i) = n->sem.iter_pos_item.iter;
                
            /* infer properties for children and
               return the resulting mapping */
            map_names (R(n), goal, np_list, att_NULL);

            /* empty the name pair list */
            PFarray_last (np_list) = 0;
            map_names (L(n), goal, np_list, att_NULL);
            return;

        case la_merge_adjacent:
            assert (n->sem.merge_adjacent.iter_res ==
                    n->sem.merge_adjacent.iter_in);
            diff_np (np_list, n->sem.merge_adjacent.item_res);
            break;

        case la_fragment:
        case la_frag_extract:
        case la_frag_union:
        case la_empty_frag:
            /* do not infer name pairs to the children */

            /* empty the name pair list */
            PFarray_last (np_list) = 0;
            break;

        case la_cond_err:
        case la_trace:
        case la_trace_msg:
        case la_trace_map:
            /* do the recursive calls by hand */
            map_names (L(n), goal, np_list, att_NULL);
            /* empty the name pair list */
            PFarray_last (np_list) = 0;
            map_names (R(n), goal, np_list, att_NULL);
            return;

        case la_nil:
            /* we do not have properties */
            break;

        case la_rec_fix:
        case la_rec_param:
        case la_rec_arg:
        case la_rec_base:
            PFoops (OOPS_FATAL,
                    "The column name tracing cannot "
                    "handle recursion operator.");
            break;
            
        case la_fun_call:
        case la_fun_param:
        case la_fun_frag_param:
            /* empty the name pair list */
            PFarray_last (np_list) = 0;
            break;

        case la_string_join:
            assert (n->sem.string_join.iter == n->sem.string_join.iter_res &&
                    n->sem.string_join.iter_sep == n->sem.string_join.iter_res);
            diff_np (np_list, n->sem.string_join.item_res);
            break;

        case la_cross_mvd:
            PFoops (OOPS_FATAL,
                    "clone column aware cross product operator is "
                    "only allowed inside mvd optimization!");
            break;

        case la_eqjoin_unq:
            PFoops (OOPS_FATAL,
                    "clone column aware equi-join operator is "
                    "only allowed with unique attribute names!");
            break;

    }

    /* infer properties for children and
       return the resulting mapping */
    if (L(n)) map_names (L(n), goal, np_list, att_NULL);
    if (R(n)) map_names (R(n), goal, np_list, att_NULL);
}

/* check for the goal operator */
static bool
find_goal (PFla_op_t *n, PFla_op_t *goal)
{
    bool found_goal = n == goal;

    assert (n);

    /* nothing to do if we already visited that node */
    if (n->bit_dag)
        return false;

    n->bit_dag = true;

    /* infer properties for children */
    for (unsigned int i = 0; i < PFLA_OP_MAXCHILD && n->child[i]; i++)
        found_goal = find_goal (n->child[i], goal) | found_goal;

    return found_goal;
}

/* reset the old property information */
static void
reset_fun (PFla_op_t *n)
{
    EDGE(n) = 0;

    /* reset the name mapping structure */
    if (n->prop->name_pairs) PFarray_last (n->prop->name_pairs) = 0;
    else n->prop->name_pairs = PFarray (sizeof (name_pair_t));
    
    if (n->prop->l_name_pairs) PFarray_last (n->prop->l_name_pairs) = 0;
}

/**
 * Trace a list of column names starting from the start
 * operator until the goal operator is reached.
 */
PFalg_attlist_t
PFprop_trace_names (PFla_op_t *start,
                    PFla_op_t *goal,
                    PFalg_attlist_t list)
{
    PFalg_attlist_t new_list;
    unsigned int    j;
    PFarray_t      *map_list = PFarray (sizeof (name_pair_t)),
                   *new_map_list;

    /* collect number of incoming edges (parents) */
    PFprop_infer_refctr (start);

    /* reset the old property information */
    PFprop_reset (start, reset_fun);

    /* check for goal */
    assert (find_goal (start, goal));
    PFla_dag_reset (start);

    /* intialize the projection list */
    for (unsigned int i = 0; i < list.count; i++)
        add_name_pair (map_list, list.atts[i], list.atts[i]);

    /* collect the mapped names */
    map_names (start, goal, map_list, att_NULL);
    new_map_list = goal->prop->name_pairs;
    assert (new_map_list);
    assert (PFarray_last (new_map_list) == list.count);

    /* create new list */
    new_list.count = list.count;
    new_list.atts  = PFmalloc (list.count * sizeof (PFalg_att_t));

    /* fill the list of mapped variable names */
    for (unsigned int i = 0; i < list.count; i++) {
        for (j = 0; j < PFarray_last (new_map_list); j++)
            if (list.atts[i] == ORI_AT(new_map_list, j)) {
                new_list.atts[i] = CUR_AT(new_map_list, j);
                break;
            }
        if (j == PFarray_last (new_map_list))
            /* fill in NULL if the name column was
               introduced after the goal operator */
            new_list.atts[i] = att_NULL;
    }

    return new_list;
}

/* vim:set shiftwidth=4 expandtab: */
