#pragma once
#include "atom.h"

/* ═══════════════════════════════════════════════════════
   Nexus — topology descriptors.
   A dag owns shape only.  Dot and arc refs are caller-owned uvlongs.

   Dots are vertices.  Arcs are directed edges.  Arc insertion rejects
   cycles, so every live dag remains acyclic.

   Ids are dense and stable for the life of the dag.  This first slice
   does not delete individual dots or arcs.  Re-adding an existing arc
   updates its ref and returns the existing arc id.

   Enumeration calls return the full count and copy at most cap ids.
   kids/pars enumerate dot ids; outs/ins enumerate arc ids.  A null buf
   is valid only when cap is zero.

   dgroots enumerates dots with no parents; dgleaves enumerates dots
   with no kids.  dgreaches reports transitive reachability between
   dots.  dgtopo enumerates dot ids in topological order.
   ═══════════════════════════════════════════════════════ */

int    mkdag(int arena);
uvlong dgdot(int dag, uvlong ref);
uvlong dgdotref(int dag, uvlong dot);
void   rdgdotref(int dag, uvlong dot, uvlong ref);
uvlong dgndot(int dag);
uvlong dgarc(int dag, uvlong from, uvlong to, uvlong ref);
uvlong dgarcref(int dag, uvlong arc);
void   rdgarcref(int dag, uvlong arc, uvlong ref);
uvlong dgnarc(int dag);
uvlong dgfrom(int dag, uvlong arc);
uvlong dgto(int dag, uvlong arc);
bool   dglinked(int dag, uvlong from, uvlong to);
uvlong dgkids(int dag, uvlong dot, uvlong *buf, uvlong cap);
uvlong dgpars(int dag, uvlong dot, uvlong *buf, uvlong cap);
uvlong dgouts(int dag, uvlong dot, uvlong *buf, uvlong cap);
uvlong dgins(int dag, uvlong dot, uvlong *buf, uvlong cap);
uvlong dgroots(int dag, uvlong *buf, uvlong cap);
uvlong dgleaves(int dag, uvlong *buf, uvlong cap);
bool   dgreaches(int dag, uvlong from, uvlong to);
uvlong dgtopo(int dag, uvlong *buf, uvlong cap);
void   rmdag(int dag);

/* ═══════════════════════════════════════════════════════
   Ugraph — undirected multigraph topology descriptors.
   A ugraph owns shape only.  Dot and arc refs are caller-owned uvlongs.

   Public ids are opaque unique identifiers.  Callers must not assume ids are
   continuous, ordered, reused, or suitable for arithmetic.  Deletion leaves
   tombstones; ids are not reused during the life of a ugraph descriptor.

   Arcs are undirected edges.  Parallel arcs and self-loops are allowed.
   ugarc always creates a new live arc.  Self-loops follow traditional graph
   degree semantics: one self-loop contributes two to ugdeg().

   ugndot and ugnarc return live counts.  ugdots and ugarcs enumerate all live
   dot or arc ids.  ugadots enumerates adjacent dots by arc multiplicity:
   parallel arcs repeat neighbors and a self-loop returns the dot twice.
   ugaarcs enumerates incident live arc ids; a self-loop arc appears once.

   Enumeration calls return the full count and copy at most cap ids.  A null
   buf is valid only when cap is zero.  Enumeration order is not a contract.

   ugdeldot deletes a live dot and cascades deletion to all incident live arcs,
   returning the number of arcs deleted.  ugdelarc deletes one live arc.
   Dead ids are errors for ref, endpoint, adjacency, connectivity, deletion,
   and component APIs.
   ═══════════════════════════════════════════════════════ */

int    mkugraph(int arena);
void   rmugraph(int graph);
uvlong ugdot(int graph, uvlong ref);
uvlong ugdotref(int graph, uvlong dot);
void   rugdotref(int graph, uvlong dot, uvlong ref);
uvlong ugndot(int graph);
uvlong ugdots(int graph, uvlong *buf, uvlong cap);
uvlong ugdeldot(int graph, uvlong dot);
uvlong ugarc(int graph, uvlong a, uvlong b, uvlong ref);
uvlong ugarcref(int graph, uvlong arc);
void   rugarcref(int graph, uvlong arc, uvlong ref);
uvlong ugnarc(int graph);
uvlong ugarcs(int graph, uvlong *buf, uvlong cap);
bool   ugends(int graph, uvlong arc, uvlong *a, uvlong *b);
bool   ugdelarc(int graph, uvlong arc);
uvlong uglinked(int graph, uvlong a, uvlong b);
uvlong ugadots(int graph, uvlong dot, uvlong *buf, uvlong cap);
uvlong ugaarcs(int graph, uvlong dot, uvlong *buf, uvlong cap);
uvlong ugdeg(int graph, uvlong dot);
bool   ugreaches(int graph, uvlong a, uvlong b);
uvlong ugcomp(int graph, uvlong dot, uvlong *buf, uvlong cap);

/* ═══════════════════════════════════════════════════════
   Mtree — ordered multi-child forest topology descriptors.
   An mtree owns shape only.  Knob and arm refs are caller-owned uvlongs.

   Knobs are tree nodes.  Arms are parent-to-kid relations.  Root knobs are
   created with mtroot(); non-root knobs are created with mtknob(), which also
   creates the incoming arm and appends the new kid to its parent's sibling
   order.  There is no public free-knob-then-attach operation.

   Public knob and arm ids are opaque unique identifiers.  Callers must not
   assume ids are continuous, ordered, reused, or suitable for arithmetic.
   Deletion leaves tombstones; ids are not reused during the life of an mtree
   descriptor.

   mtknobs and mtroots enumerate live knob ids.  mtkids enumerates live kid
   knobs in sibling order.  mtkidarms enumerates the incoming arm ids for those
   same kids in the same order.  mtsubtree enumerates the live subtree rooted at
   a knob.  Enumeration calls return the full count and copy at most cap ids.
   A null buf is valid only when cap is zero.  Global/root/subtree order is not
   a contract; sibling order is.

   mtpar() and mtinarm() are errors for roots.  mtdetach() rejects roots,
   tombstones the incoming arm, and makes the kid subtree a root.  mtmove()
   rejects roots, dead ids, self-parenting, and moves under descendants.  Moving
   to the same parent succeeds as a no-op; moving to a new parent appends at
   tail and preserves the incoming arm id/ref.  mtdelknob() deletes a knob's
   whole subtree and returns the number of knobs deleted.
   ═══════════════════════════════════════════════════════ */

int    mkmtree(int arena);
void   rmmtree(int tree);
uvlong mtroot(int tree, uvlong ref);
uvlong mtknob(int tree, uvlong par, uvlong ref, uvlong armref);
uvlong mtknobref(int tree, uvlong knob);
void   rmtknobref(int tree, uvlong knob, uvlong ref);
uvlong mtnknob(int tree);
uvlong mtknobs(int tree, uvlong *buf, uvlong cap);
uvlong mtdelknob(int tree, uvlong knob);
uvlong mtarmref(int tree, uvlong arm);
void   rmtarmref(int tree, uvlong arm, uvlong ref);
uvlong mtnarm(int tree);
bool   mtisroot(int tree, uvlong knob);
uvlong mtroots(int tree, uvlong *buf, uvlong cap);
uvlong mtpar(int tree, uvlong kid);
uvlong mtinarm(int tree, uvlong kid);
uvlong mtkids(int tree, uvlong par, uvlong *buf, uvlong cap);
uvlong mtkidarms(int tree, uvlong par, uvlong *buf, uvlong cap);
bool   mtancestor(int tree, uvlong anc, uvlong kid);
uvlong mtsubtree(int tree, uvlong knob, uvlong *buf, uvlong cap);
bool   mtdetach(int tree, uvlong kid);
bool   mtmove(int tree, uvlong kid, uvlong newpar);
