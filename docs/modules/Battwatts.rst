.. _Battwatts:

Battwatts
**************************

Wrapper for SAM Simulation Core model: `cmod_battwatts.cpp <https://github.com/NREL/ssc/blob/develop/ssc/cmod_battwatts.cpp>`_

Creating an Instance
=========================

There are three methods to create a new instance of a PySAM module. Using ``default`` populates the newclass' attributes with default values specific to a ``config``. Each technology-financialconfiguration corresponds to a SAM GUI configuration. Using ``new`` creates an instance with empty attributes. The ``wrap`` function allows compatibility with PySSC, for details, refer to :doc:`../PySSC`.

**Battwatts model description**

.. automodule:: PySAM.Battwatts
	:members:

Functions
=========================

.. autoclass:: PySAM.Battwatts.Battwatts
	:members:

PVWatts Group
==============

.. autoclass:: PySAM.Battwatts.Battwatts.PVWatts
	:members:

FinancialAnalysisParameters Group
==============

.. autoclass:: PySAM.Battwatts.Battwatts.FinancialAnalysisParameters
	:members:

BatteryModelSimple Group
==============

.. autoclass:: PySAM.Battwatts.Battwatts.BatteryModelSimple
	:members:

Common Group
==============

.. autoclass:: PySAM.Battwatts.Battwatts.Common
	:members:

ElectricLoadOther Group
==============

.. autoclass:: PySAM.Battwatts.Battwatts.ElectricLoadOther
	:members:

Outputs Group
==============

.. autoclass:: PySAM.Battwatts.Battwatts.Outputs
	:members:

