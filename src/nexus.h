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

/* ═══════════════════════════════════════════════════════
   Lattice — fixed 2D grid topology descriptors.
   A lattice owns rectangular shape and one caller-owned uvlong phi per cell.

   Coordinates are mathematical: x grows right, y grows upward, and vlong
   bounds may be negative.  mklattice() uses inclusive bounds
   [xmin,xmax] × [ymin,ymax].  Every cell inside those bounds exists from
   construction; there is no occupancy, add-cell, delete-cell, public cell id,
   public z-index, or full-grid traversal API.

   Each cell starts with the constructor init phi.  ltbase() returns that init
   value.  ltclear() resets every cell to it.  ltphis() enumerates only cells
   whose phi differs from ltbase(), returning ltcell records.  Its order is not
   a public contract.

   Phi storage is compact and internally ordered by a clipped Morton layout for
   locality.  This layout is not exposed and callers must use coordinates.

   ltbounds() is a query with optional output pointers: null outputs are
   silently skipped.  ltorth() returns in-bounds orthogonal neighbor phis in
   counterclockwise order from east: E, N, W, S.  ltsurr() returns surrounding
   neighbor phis in counterclockwise order from east: E, NE, N, NW, W, SW, S,
   SE.  ltorth(), ltsurr(), and ltphis() use the count/cap/null-buffer
   convention; null buffers are valid only when cap is zero.
   ═══════════════════════════════════════════════════════ */

typedef struct ltcell {
	vlong  x;
	vlong  y;
	uvlong phi;
} ltcell;

int    mklattice(int arena, vlong xmin, vlong xmax, vlong ymin, vlong ymax, uvlong init);
void   rmlattice(int lattice);
void   ltbounds(int lattice, vlong *xmin, vlong *xmax, vlong *ymin, vlong *ymax);
uvlong ltbase(int lattice);
uvlong ltphi(int lattice, vlong x, vlong y);
void   rltphi(int lattice, vlong x, vlong y, uvlong phi);
void   ltclear(int lattice);
uvlong ltorth(int lattice, vlong x, vlong y, uvlong *buf, uvlong cap);
uvlong ltsurr(int lattice, vlong x, vlong y, uvlong *buf, uvlong cap);
uvlong ltphis(int lattice, ltcell *buf, uvlong cap);

/* ═══════════════════════════════════════════════════════
   List — descriptorless xor-linked topology.
   A list owns shape only.  Knots are arena-allocated two-field records:
   yod is the caller-owned uvlong ref and vav is xor adjacency.  Public knot
   handles are knot addresses cast to uvlong; 0 is null.  There is no list
   descriptor, head/tail record, link ref, or validity registry.  Invalid
   nonzero knot handles are fail-fast undefined behavior.

   bead describes a directed list segment.  pre is the outside predecessor
   used to enter fst; pre is not part of the bead.  Walking with
   lsstep(previous,current) from pre,fst visits knots through lst inclusive.

   mklist batch-creates an independent list in an arena from yods[0..n) and
   returns {0, first, last}.  n==0 returns the empty bead {0,0,0}.  Empty beads
   are the identity for lscut(), lsput(), lscat(), and lssplice().

   lsput inserts an independent bead after at in direction atpre -> at.
   lscat finds the directional end first, then puts there.  lscut detaches a
   bead from its surrounding component and returns it as independent.
   lssplice is lscut followed by lsput.  rmlist enters through u,v (where
   either may be 0) and clears the connected component by zeroing reached
   knots; arena memory is reclaimed only by rmarena().

   lsknots uses the count/cap/null-buffer convention.  lsend has an optional
   endpre output pointer; null is silently skipped.
   ═══════════════════════════════════════════════════════ */

typedef struct bead {
	uvlong pre;
	uvlong fst;
	uvlong lst;
} bead;

bead   mklist(int arena, uvlong *yods, uvlong n);
uvlong lsyod(uvlong knot);
void   rlsyod(uvlong knot, uvlong yod);
uvlong lsstep(uvlong pre, uvlong knot);
uvlong lsend(uvlong pre, uvlong knot, uvlong *endpre);
uvlong lslen(uvlong pre, uvlong knot);
uvlong lsknots(uvlong pre, uvlong knot, uvlong *buf, uvlong cap);
bead   lscut(bead b);
void   lsput(uvlong atpre, uvlong at, bead b);
void   lscat(uvlong pre, uvlong knot, bead b);
void   lssplice(uvlong atpre, uvlong at, bead b);
void   rmlist(int arena, uvlong u, uvlong v);

/* ═══════════════════════════════════════════════════════
   Bitree — descriptorless binary tree topology.
   A bitree owns shape only.  Knods are arena-allocated records with parent,
   caller-owned uvlong ref, and two child slots.  Public knod handles are knod
   addresses cast to uvlong; 0 is null.  There is no tree descriptor, root
   record, edge ref, validity registry, or tombstone table.  Invalid nonzero
   handles are fail-fast undefined behavior.

   kids[0] is zero and kids[1] is one.  Callers own current root handles.
   Operations that change a subtree root return the new or detached handle.

   rbtkids batch-creates zero/one children: a non-null input pointer contains
   the child ref and is overwritten with the new child handle on success.
   Null side pointers are skipped.  btkids uses optional output pointers in
   the same style.  btdrop detaches par->kids[side]; dropping an empty slot is
   a quiet empty result.  btplace places a nonzero detached kid and returns the
   replaced child if any.  rmbitree clears the subtree rooted at root; if root
   has a parent, the parent's corresponding child slot is cleared first.

   btside returns false for zero and true for one.  Root/detached knods return
   false and set errmsg(); other side-query errors return true and set errmsg().
   btrot promotes the child on side.  btrrotte rotates the child on second by
   first, then rotates the knod by second.

   btwalk enumerates knod handles with order 0 preorder, 1 inorder, 2
   postorder, and 3 level order.  It uses the count/cap/null-buffer convention.
   ═══════════════════════════════════════════════════════ */

uvlong mkbitree(int arena, uvlong ref);
void   rmbitree(uvlong root);
uvlong btref(uvlong knod);
void   rbtref(uvlong knod, uvlong ref);
uvlong btpar(uvlong knod);
bool   btside(uvlong knod);
void   btkids(uvlong knod, uvlong *zero, uvlong *one);
void   rbtkids(int arena, uvlong knod, uvlong *zero, uvlong *one);
uvlong btdrop(uvlong par, int side);
uvlong btplace(uvlong par, int side, uvlong kid);
uvlong btrot(uvlong knod, int side);
uvlong btrrotte(uvlong knod, int first, int second);
uvlong btwalk(uvlong root, int order, uvlong *buf, uvlong cap);

/* ═══════════════════════════════════════════════════════
   Indeltree — ordered stateful topology descriptors.
   An indeltree owns ordered red-black tree shape only.  Knods store
   caller-owned uvlong refs; ordering is supplied per operation by a comparator
   over ref-domain values.  Public knod ids are opaque encoded handles and are
   not bitree handles.  Callers must not inspect or mutate tree shape.

   Duplicates are allowed.  inplace always creates a new knod and inserts it
   after existing equals in inorder order.  infind returns the first equal knod;
   exact miss is a quiet false result.  inlower returns the first knod >= chet
   and inupper returns the first knod > chet; no such bound is an error.

   infirst/inlast return ordered ends; empty trees are errors.  innext/inprev
   return false without error at ordered endpoints and silently skip a null
   output pointer.  indrop deletes one live knod by identity.  indels keeps the
   first exceed equal knods for chet and deletes the rest, returning the deleted
   count; no equal range is a quiet zero result.

   inknods enumerates live knod ids in inorder with the count/cap/null-buffer
   convention.
   ═══════════════════════════════════════════════════════ */

int    mkindeltree(int arena);
void   rmindeltree(int tree);
uvlong inplace(int tree, uvlong ref, int (*cmp)(uvlong ref, uvlong chet, void *arg), void *arg);
bool   indrop(int tree, uvlong knod);
uvlong indels(int tree, uvlong chet, uvlong exceed, int (*cmp)(uvlong ref, uvlong chet, void *arg), void *arg);
uvlong inref(int tree, uvlong knod);
uvlong innknod(int tree);
uvlong inknods(int tree, uvlong *buf, uvlong cap);
bool   infind(int tree, uvlong chet, int (*cmp)(uvlong ref, uvlong chet, void *arg), void *arg, uvlong *knod);
uvlong inlower(int tree, uvlong chet, int (*cmp)(uvlong ref, uvlong chet, void *arg), void *arg);
uvlong inupper(int tree, uvlong chet, int (*cmp)(uvlong ref, uvlong chet, void *arg), void *arg);
uvlong infirst(int tree);
uvlong inlast(int tree);
bool   innext(int tree, uvlong knod, uvlong *next);
bool   inprev(int tree, uvlong knod, uvlong *prev);

/* ═══════════════════════════════════════════════════════
   Skiplist — ordered randomized skiplist topology descriptors.
   A skiplist owns shape only.  Pins are ordered elements with caller-owned
   uvlong yods.  Ordering is supplied per operation by a comparator over yod
   domain values; chet is the caller-provided comparison value.

   Public pin ids are opaque unique identifiers.  Callers must not assume ids
   are continuous, ordered, reused, or suitable for arithmetic.  Deletion leaves
   tombstones; ids are not reused during the life of a skiplist descriptor.

   Internally each layer is an xor list with one private head knot.  Layer knots
   store pin ids as their yod.  Layers, internal knots, and pin heights are not
   public API.

   skput inserts after existing equals and returns the new pin.  skfind returns
   false quietly on exact miss.  sklower returns the first pin >= chet; skupper
   returns the first pin > chet.

   skfirst/sklast return the ordered first/last pin; empty skiplists are errors.
   sknext/skprev return false without error at ordered ends and silently skip a
   null output pointer.  skdel deletes one live pin by identity.  skdels starts
   at a live pin and deletes the forward contiguous run comparing equal to that
   pin's yod.

   skpins enumerates live pins in bottom-layer sorted order with the
   count/cap/null-buffer convention.
   ═══════════════════════════════════════════════════════ */

int    mkskiplist(int arena);
void   rmskiplist(int skip);
uvlong skput(int skip, uvlong yod, int (*cmp)(uvlong yod, uvlong chet, void *arg), void *arg);
bool   skdel(int skip, uvlong pin);
uvlong skdels(int skip, uvlong pin, int (*cmp)(uvlong yod, uvlong chet, void *arg), void *arg);
uvlong skyod(int skip, uvlong pin);
uvlong sknpin(int skip);
uvlong skpins(int skip, uvlong *buf, uvlong cap);
bool   skfind(int skip, uvlong chet, int (*cmp)(uvlong yod, uvlong chet, void *arg), void *arg, uvlong *pin);
uvlong sklower(int skip, uvlong chet, int (*cmp)(uvlong yod, uvlong chet, void *arg), void *arg);
uvlong skupper(int skip, uvlong chet, int (*cmp)(uvlong yod, uvlong chet, void *arg), void *arg);
uvlong skfirst(int skip);
uvlong sklast(int skip);
bool   sknext(int skip, uvlong pin, uvlong *next);
bool   skprev(int skip, uvlong pin, uvlong *prev);
