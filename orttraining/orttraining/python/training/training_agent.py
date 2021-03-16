# -------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# --------------------------------------------------------------------------

import onnxruntime
from onnxruntime.capi import _pybind_state as C
from onnxruntime.capi.onnxruntime_inference_collection import IOBinding, OrtValue
from onnxruntime.capi._pybind_state import TrainingAgent as C_TrainingAgent


class TrainingAgent(object):
    """
    This is the main class used to run a ORTModule model.
    """

    def __init__(self, path_or_bytes, session_options=None, providers=None, provider_options=None):
        """
        :param path_or_bytes: filename or serialized ONNX or ORT format model in a byte string
        :param sess_options: session options
        :param providers: Optional sequence of providers in order of decreasing
            precedence. Values can either be provider names or tuples of
            (provider name, options dict). If not provided, then all available
            providers are used with the default precedence.
        :param provider_options: Optional sequence of options dicts corresponding
            to the providers listed in 'providers'.

        The model type will be inferred unless explicitly set in the SessionOptions.
        To explicitly set:
          so = onnxruntime.SessionOptions()
          so.add_session_config_entry('session.load_model_format', 'ONNX') or
          so.add_session_config_entry('session.load_model_format', 'ORT') or

        A file extension of '.ort' will be inferred as an ORT format model.
        All other filenames are assumed to be ONNX format models.

        'providers' can contain either names or names and options. When any options
        are given in 'providers', 'provider_options' should not be used.

        The list of providers is ordered by precedence. For example ['CUDAExecutionProvider', 'CPUExecutionProvider']
        means execute a node using CUDAExecutionProvider if capable, otherwise execute using CPUExecutionProvider.
        """

        self._training_agent = None
        self._inference_session = None

        self.create_training_agent(path_or_bytes, session_options, providers, provider_options)


    def create_training_agent(self, path_or_bytes, session_options, providers, provider_options):
        self._inference_session = onnxruntime.InferenceSession(path_or_bytes, session_options,
                                                               providers, provider_options)
        self._training_agent = C_TrainingAgent(self._inference_session._sess)

    def io_binding(self):
        "Return an onnxruntime.IOBinding object`."
        return IOBinding(self._inference_session)

    def run_forward(self, iobinding, run_options):
        """
         Compute the forward subgraph until it hits the Yield or Hole Op.
         :param iobinding: the iobinding object that has graph inputs/outputs bind.
         :param run_options: See :class:`onnxruntime.RunOptions`.
        """
        ortvalues, run_id, token_id = self._training_agent.run_forward(iobinding._iobinding, run_options)
        return [OrtValue(ortvalue) for ortvalue in ortvalues], run_id, token_id

    def resume_forward(self, resume_inputs, run_id):
        """
         Compute the forward subgraph until it hits the Yield or Hole Op.
         :param iobinding: the iobinding object that has graph inputs/outputs bind.
         :param run_options: See :class:`onnxruntime.RunOptions`.
        """
        ortvalues, run_id, token_id = self._training_agent.resume_forward(resume_inputs, run_id)
        return [OrtValue(ortvalue) for ortvalue in ortvalues], run_id, token_id

    def run_backward(self, backward_output_grads, run_id):
        """
         Resume executing the backward subgraph starting from Yield Op.
         :param backward_output_grads: Output gradients for backward.
        """
        ortvalues, run_id, token_id = self._training_agent.run_backward(backward_output_grads, run_id)
        return [OrtValue(ortvalue) for ortvalue in ortvalues], run_id, token_id

    def resume_backward(self, resume_inputs, run_id):
        """
         Resume executing the backward subgraph starting from Yield Op.
         :param backward_output_grads: Output gradients for backward.
        """
        ortvalues, run_id, token_id = self._training_agent.run_backward(resume_inputs, run_id)
        return [OrtValue(ortvalue) for ortvalue in ortvalues], run_id, token_id