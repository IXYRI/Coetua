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
