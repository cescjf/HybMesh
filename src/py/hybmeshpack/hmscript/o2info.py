" 2D object information"
from hybmeshpack.hmscript import flow, hmscriptfun
from hybmeshpack.hmcore import c2 as c2core
from hybmeshpack.gdata.cont2 import Contour2, closest_contour
from datachecks import (icheck, List, Bool, Point2D, Grid2D, UInt, ACont2D,
                        OneOf, Cont2D, Float, NoneOr, InvalidArgument)


@hmscriptfun
def info_grid(gid):
    """Get grid structure information

    :param gid: grid identifier

    :returns: dictionary which represents
       total number of nodes/cells/edges
       and number of cells of each type::

         {'Nnodes': int,
          'Ncells': int,
          'Nedges': int,
          'cell_types': {int: int}  # cell dimension: number of such cells
          }

    """
    icheck(0, Grid2D())
    grid = flow.receiver.get_grid2(gid)
    ret = {}
    ret['Nnodes'] = grid.n_vertices()
    ret['Ncells'] = grid.n_cells()
    ret['Nedges'] = grid.n_edges()
    ret['cell_types'] = grid.cell_types_info()
    return ret


@hmscriptfun
def info_contour(cid):
    """Get contour structure information

    :param cid: contour or grid identifier

    :returns:
       dictionary representing contour length, total number of nodes, edges,
       number of edges in each subcontour and number of edges
       of each boundary type::

         {'Nnodes': int,
          'Nedges': int,
          'subcont': [list-of-int],    # edges number in each subcontour
          'btypes': {btype(int): int}  # boundary type: number of edges
          'length': float
         }

    """
    icheck(0, ACont2D())

    cont = flow.receiver.get_any_contour(cid).contour2()
    ret = {}
    ret['Nnodes'] = cont.n_vertices()
    ret['Nedges'] = cont.n_edges()

    sep = [Contour2(s) for s in c2core.quick_separate(cont.cdata)]
    ret['subcont'] = [s.n_edges() for s in sep]

    bt = cont.raw_data('bt')
    ret['btypes'] = {}
    for s in bt:
        if s not in ret['btypes']:
            ret['btypes'][s] = 1
        else:
            ret['btypes'][s] += 1
    ret['length'] = cont.length()
    return ret


@hmscriptfun
def get_point(obj, ind=None, vclosest=None, eclosest=None, cclosest=None,
              only_contour=True):
    """ Returns object point

    :param obj: grid or contour identifier

    :param int ind: index of point

    :param vclosest:

    :param eclosest:

    :param cclosest: point as [x, y] list

    :param bool only_contour: If that is true then if **objs** is a grid
       then respective grid contour will be used

    :returns: point as [x, y] list

    Only one of **ind**, **vclosest**, **eclosest**, **cclosest**
    arguments should be defined.

    If **ind** is defined then returns point at given index.

    If **vvlosest** point is defined then returns object vertex closest to
    this point.

    If **eclosest** point is defined then returns point owned by an
    object edge closest to input point.

    If **cclosest** point is defined then returns non straight line
    object contour vertex closest to input point.
    """
    icheck(0, ACont2D())
    icheck(1, NoneOr(UInt()))
    icheck(2, NoneOr(Point2D()))
    icheck(3, NoneOr(Point2D()))
    icheck(4, NoneOr(Point2D()))
    icheck(5, Bool())
    if ind is None and vclosest is None and eclosest is None and\
            cclosest is None:
        raise InvalidArgument("Define point location")

    try:
        tar = flow.receiver.get_contour2(obj)
    except KeyError:
        tar = flow.receiver.get_grid2(obj)
        if only_contour or cclosest is not None:
            tar = tar.contour()
    if cclosest is not None:
        tar = tar.deepcopy()
        c2core.simplify(tar.cdata, 0, False)
        vclosest = cclosest

    if vclosest is not None:
        return tar.closest_points([vclosest], "vertex")[0]
    elif eclosest is not None:
        return tar.closest_points([eclosest], "edge")[0]
    elif ind is not None:
        return tar.point_at(ind)


@hmscriptfun
def domain_area(cid):
    """Calculates area of the domain bounded by the contour

    :param cid: grid or contour identifier

    :returns: positive float or zero for open contours
    """
    icheck(0, ACont2D())
    return flow.receiver.get_any_contour(cid).area()


@hmscriptfun
def pick_contour(pnt, contlist=[]):
    """ Returns contour closest to given point

    :param pnt: point as [x, y] list

    :param contlist: list of contour identifier to choose from

    :returns: closest contour identifier

    If **contlist** is empty then looks over all registered contours.
    This procedure does not take 2d grid contours into account.
    """
    icheck(0, Point2D())
    icheck(1, List(ACont2D()))

    conts = map(flow.receiver.get_any_contour, contlist)
    cc = closest_contour(conts, pnt)
    ind = conts.index(cc)
    return contlist[ind]


@hmscriptfun
def skewness(gid, threshold=0.7):
    """Reports equiangular skewness coefficient (in [0, 1]) of grid cells

    :param gid: grid identifier

    :param float threshold: cells with skewness greater than this
       value are considered bad and will be reported.
       Set it to -1 to get skewness for each cell.

    :returns:
       dictionary with keys::

         {'ok': bool,                  # True if no bad_cells were found
          'max_skew': float,           # maximum skew value in grid
          'max_skew_cell': int,        # index of cell with maximum skew
          'bad_cells': [list-of-int],  # list of bad cell indicies
          'bad_skew': [list-of-float]  # list of bad cell skew values
          }

    Respective `bad_cells` and `bad_skew` lists entries correspond
    to the same cells.
    """
    icheck(0, Grid2D())
    icheck(1, Float())

    skew = flow.receiver.get_grid2(gid).skewness(threshold)
    if (len(skew) == 0):
        raise Exception("Failed to calculate skewness")
    skew['ok'] = (len(skew['bad_cells']) == 0)
    return skew


@hmscriptfun
def tab_cont2(obj, what):
    """ Returns plain table for the given contour.

    :param str obj: contour identifier

    :param str what: table name

    :returns: plain ctypes array representing requested table.

    Possible **what** values are:

    * ``'vert'`` - vertex coordinates table,
    * ``'edge_vert'`` - edge-vertex connectivity: indices of first and
      last vertices for each edge,
    * ``'bt'`` - boundary features of each edge.

    """
    icheck(0, Cont2D())
    icheck(1, OneOf('vert',
                    'edge_vert',
                    'bt'))
    return flow.receiver.get_contour2(obj).raw_data(what)


@hmscriptfun
def tab_grid2(obj, what):
    """ Returns plain table for the given grid.

    :param str obj: grid identifier

    :param str what: table name

    :returns: plain ctypes array representing requested table.

    Possible **what** values are:

    * ``'vert'`` - vertex coordinates table,
    * ``'edge_vert'`` - edge-vertex connectivity: indices of first and
      last vertices for each edge,
    * ``'edge_cell'`` - edge-cell connectivity: indices of left and right
      cell for each edge. If this is a boundary edge ``-1`` is used to mark
      boundary edge side,
    * ``'cell_dim'`` - number of vertices in each cell,
    * ``'cell_edge'`` - cell-edge connectivity: counterclockwise ordered 
      edge indices for each cell.
    * ``'cell_vert'`` - cell-vertex connectivity: counterclockwise ordered 
      vertex indices for each cell.
    * ``'bnd'`` - list of boundary edges indices,
    * ``'bt'`` - boundary types for all edges including internal ones,
    * ``'bnd_bt'`` - (boundary edge, boundary feature) pairs

    In case of grids with variable cell dimensions
    ``'cell_edge'`` and ``'cell_vert'`` tables require
    additional ``'cell_dim'`` table to subdive
    returned plain array by certain cells.
    """
    icheck(0, Grid2D())
    icheck(1, OneOf('vert',
                    'edge_vert', 'edge_cell',
                    'cell_dim', 'cell_edge', 'cell_vert',
                    'bnd', 'bt', 'bnd_bt',
                    ))
    return flow.receiver.get_grid2(obj).raw_data(what)
