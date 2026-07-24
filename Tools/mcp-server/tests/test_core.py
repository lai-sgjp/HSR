# Copyright (c) 2025 GenOrca. All Rights Reserved.

"""Unit tests for the TCP response unwrapping (offline, no Unreal)."""

from unreal_mcp.core import _unwrap_result


def test_unwraps_action_dict_from_result_string():
    wire = {
        "success": True,  # python-exec success, NOT action success
        "message": "Python command executed successfully.",
        "result": '{"success": true, "actor_label": "PointLight_2"}',
    }
    out = _unwrap_result(wire)
    assert out == {"success": True, "actor_label": "PointLight_2"}


def test_unwrapped_inner_failure_is_surfaced():
    """Action failure (inner success=False) must survive unwrapping as data."""
    wire = {
        "success": True,  # python ran fine...
        "result": '{"success": false, "message": "Actor not found"}',
    }
    out = _unwrap_result(wire)
    assert out["success"] is False
    assert out["message"] == "Actor not found"


def test_unwraps_list_result():
    wire = {"success": True, "result": '[1, 2, 3]'}
    assert _unwrap_result(wire) == [1, 2, 3]


def test_non_json_result_returns_original():
    wire = {"success": True, "result": "not json at all"}
    assert _unwrap_result(wire) == wire


def test_missing_result_returns_original():
    wire = {"success": True, "message": "ok"}
    assert _unwrap_result(wire) == wire


def test_non_dict_passthrough():
    assert _unwrap_result("raw") == "raw"
