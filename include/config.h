#pragma once

/**
 * @brief If defined, `std::shared_ptr` will be used for Abstract Syntax Tree (AST) node references (`ASTRef`).
 * This provides automatic memory management for AST nodes.
 */
#define NG_CONFIG_USING_SHARED_PTR_FOR_AST

/**
 * @brief If defined, debug logging will be enabled throughout the application.
 * This is useful for development and troubleshooting, but should be disabled in production for performance reasons.
 */
#define NG_CONFIG_ENABLE_DEBUG_LOG
