from uuid import uuid4
import numpy as np
import ROOT


class Hist1D:
    """NumPy-based representation of 1D histogram.
    
    Includes under- and overflow bins.
    """
    
    def __init__(self, *args, binning=None, contents=None, errors=None):
        """Construct from binning or ROOT histogram."""
        
        if len(args) in [0, 3]:
            if len(args) == 3:
                # Assume arguments define an equidistant binning
                self.binning = np.linspace(args[1], args[2], num=args[0])
                
                if binning is not None:
                    raise RuntimeError('Conflicting definitions of binning.')
            else:
                if binning is None:
                    raise RuntimeError('Binning must be provided.')

                self.binning = np.asarray(binning, dtype=np.float64)
            
            # With the binning specified, set bin contents and errors
            self.contents = np.zeros(len(self.binning) + 1, dtype=np.float64)
            self.errors = np.zeros_like(self.contents)
            
            if contents is not None:
                if len(contents) == len(self.contents):
                    self.contents[:] = contents
                elif len(contents) == len(self.contents) - 2:
                    # Assume under- and overflows are missing
                    self.contents[1:-1] = contents
                else:
                    raise RuntimeError('Unexpected length of array of bin contents.')
            
            if errors is not None:
                if len(errors) == len(self.errors):
                    self.errors[:] = errors
                elif len(errors) == len(self.errors) - 2:
                    # Assume under- and overflows are missing
                    self.errors[1:-1] = errors
                else:
                    raise RuntimeError('Unexpected length of array of bin errors.')
                
                if contents is not None and len(errors) != len(contents):
                    raise RuntimeError('Inconsistent arrays of bin contents and errors.')
            
            elif contents is not None:
                self.errors = np.sqrt(self.contents)
        
        elif len(args) == 1:
            # Initialize from a ROOT histogram
            if not isinstance(args[0], ROOT.TH1):
                raise TypeError('ROOT histogram expected, got {}.'.format(type(args[0])))
            
            hist = args[0]
            
            if hist.GetDimension() != 1:
                raise RuntimeError('1D histogram is expected.')
            
            numbins = hist.GetNbinsX()
            self.binning = np.zeros(numbins + 1, dtype=np.float64)
            self.contents = np.zeros(numbins + 2, dtype=np.float64)
            self.errors = np.zeros_like(self.contents)
            
            for bin in range(1, numbins + 2):
                self.binning[bin - 1] = hist.GetBinLowEdge(bin)
            
            for bin in range(numbins + 2):
                self.contents[bin] = hist.GetBinContent(bin)
                self.errors[bin] = hist.GetBinError(bin)
    
    
    @property
    def numbins(self):
        """Return number of bins in the histogram.
        
        Under- and overflow bins are not counted.
        """
        
        return len(self.binning) - 1


    def to_root(self, name=None):
        """Convert to a ROOT histogram.

        Create a new ROOT.TH1D with the same content as self.  It is not
        associtated with any ROOT directory.

        Arguments:
            name:  Name for the ROOT histogram.  If not given, use a
                unique random name.

        Return value:
            Instance of ROOT.TH1D.
        """

        if not name:
            name = uuid4().hex

        hist = ROOT.TH1D(name, '', len(self.binning) - 1, self.binning)

        for b in range(self.numbins + 2):
            hist.SetBinContent(b, self.contents[b])
            hist.SetBinError(b, self.errors[b])

        return hist
    
    
    def __getitem__(self, index):
        """Return content and error for bin with given index.
        
        Under- and overflow bins have indices 0 and -1.
        """
        
        return self.contents[index], self.errors[index]


def spline_to_root(spline):
    """Convert a SciPy spline into ROOT.TSpline3.
    
    Arguments:
        spline:  SciPy one-dimensional spline such as
            scipy.interpolate.UnivariateSpline.

    Return value:
        Equivalent ROOT.TSpline3.
    """
    
    # Force the ROOT spline to pass through all knots and provide
    # boundary conditions for second derivatives.
    knots = spline.get_knots()
    boundaries_der2 = spline.derivative(2)(knots[[0, -1]])
    root_spline = ROOT.TSpline3(
        '', knots, spline(knots), len(knots),
        'b2 e2', boundaries_der2[0], boundaries_der2[-1]
    )
    
    return root_spline


mpl_style = {
    'figure.figsize': (6.0, 4.8),
    
    'axes.labelsize':              'large',
    'axes.formatter.use_mathtext': True,
    'axes.formatter.limits':       (-2, 4),
    
    'xtick.top':          True,
    'xtick.direction':    'in',
    'xtick.minor.top':    True,
    'xtick.minor.bottom': True,
    
    'ytick.right':        True,
    'ytick.direction':    'in',
    'ytick.minor.left':   True,
    'ytick.minor.right':  True,
    
    'lines.linewidth':   1.,
    'lines.markersize':  3.,
    'errorbar.capsize':  1.
}