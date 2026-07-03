INSERT INTO `model_config` (`model_id`, `model_name`, `protocol`, `default_timeout_ms`, `max_read_bit_block_size`, `max_read_register_block_size`, `max_write_bit_block_size`, `max_write_register_block_size`) VALUES

-- Aggregated. Aggregation
(102, 'Aggregated Aggregator 1', 'aggregated', NULL, 0, 0, 0, 0),
(103, 'Aggregated Aggregator 2', 'aggregated', NULL, 0, 0, 0, 0),
(104, 'Aggregated Aggregator 3', 'aggregated', NULL, 0, 0, 0, 0),
(105, 'Aggregated Aggregator 4', 'aggregated', NULL, 0, 0, 0, 0),
(106, 'Meta Aggregated Aggregator 5', 'aggregated', NULL, 0, 0, 0, 0),
(107, 'Meta Aggregated Aggregator 6', 'aggregated', NULL, 0, 0, 0, 0),
(108, 'Meta Aggregated Aggregator 7', 'aggregated', NULL, 0, 0, 0, 0);


INSERT INTO `model_io_definition`
(`fk_model_id`, `logical_address`, `io_type`, `purpose`, `hardware_access`, `physical_address`, `access_method`, `bitmask_offset`, `register_count`, `endianess`, `default_io_label`)
VALUES

-- ============================ AGGREGATED MODULES ============================
-- Note: physical_address for aggregated models is set to 0 as it is not used.
-- Model 104: Aggregated_Plant_IO_2 (16 Outputs, 8 Secure States)
(104, 0, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 1'),
(104, 1, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 2'),
(104, 2, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 3'),
(104, 3, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 4'),
(104, 4, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 5'),
(104, 5, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 6'),
(104, 6, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 7'),
(104, 7, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 8'),
(104, 8, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 9'),
(104, 9, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 10'),
(104, 10, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 11'),
(104, 11, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 12'),
(104, 12, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 13'),
(104, 13, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 14'),
(104, 14, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 15'),
(104, 15, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 16'),
(104, 16, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 1'),
(104, 17, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 2'),
(104, 18, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 3'),
(104, 19, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 4'),
(104, 20, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 5'),
(104, 21, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 6'),
(104, 22, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 7'),
(104, 23, 'bit', 'secure_state', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Safe State 8'),
(104, 24, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 0, 1, 'little', 'Safe State Output 1'),
(104, 25, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 1, 1, 'little', 'Safe State Output 2'),
(104, 26, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 2, 1, 'little', 'Safe State Output 3'),
(104, 27, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 3, 1, 'little', 'Safe State Output 4'),
(104, 28, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 4, 1, 'little', 'Safe State Output 5'),
(104, 29, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 5, 1, 'little', 'Safe State Output 6'),
(104, 30, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 6, 1, 'little', 'Safe State Output 7'),
(104, 31, 'register', 'secure_state', 'readwrite', 16, 'bitmask', 7, 1, 'little', 'Safe State Output 8'),
-- watchdog timer
(104, 32, 'register', 'config', 'readwrite', NULL, 'direct', 0, 1, 'little', 'WDT Timeout'),

-- Model 105: Aggregated_Plant_IO_3 (8 Outputs)
(105, 0, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 1'),
(105, 1, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 2'),
(105, 2, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 3'),
(105, 3, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 4'),
(105, 4, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 5'),
(105, 5, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 6'),
(105, 6, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 7'),
(105, 7, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 8'),

-- Model 106: Aggregated_IO (32 Outputs)
(106, 0, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 1'),
(106, 1, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 2'),
(106, 2, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 3'),
(106, 3, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 4'),
(106, 4, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 5'),
(106, 5, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 6'),
(106, 6, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 7'),
(106, 7, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 8'),
(106, 8, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 9'),
(106, 9, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 10'),
(106, 10, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 11'),
(106, 11, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 12'),
(106, 12, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 13'),
(106, 13, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 14'),
(106, 14, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 15'),
(106, 15, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 16'),
(106, 16, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 17'),
(106, 17, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 18'),
(106, 18, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 19'),
(106, 19, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 20'),
(106, 20, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 21'),
(106, 21, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 22'),
(106, 22, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 23'),
(106, 23, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 24'),
(106, 24, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 25'),
(106, 25, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 26'),
(106, 26, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 27'),
(106, 27, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 28'),
(106, 28, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 29'),
(106, 29, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 30'),
(106, 30, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 31'),
(106, 31, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 32'),

-- Model 107: Aggregated_IO_2 (16 Outputs)
(107, 0, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 1'),
(107, 1, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 2'),
(107, 2, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 3'),
(107, 3, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 4'),
(107, 4, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 5'),
(107, 5, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 6'),
(107, 6, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 7'),
(107, 7, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 8'),
(107, 8, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 9'),
(107, 9, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 10'),
(107, 10, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 11'),
(107, 11, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 12'),
(107, 12, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 13'),
(107, 13, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 14'),
(107, 14, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 15'),
(107, 15, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 16'),

-- Model 108: Meta_Aggregated_IO_4 (48 Outputs, 8 Inputs)
(108, 0, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 1'),
(108, 1, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 2'),
(108, 2, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 3'),
(108, 3, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 4'),
(108, 4, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 5'),
(108, 5, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 6'),
(108, 6, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 7'),
(108, 7, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 8'),
(108, 8, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 9'),
(108, 9, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 10'),
(108, 10, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 11'),
(108, 11, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 12'),
(108, 12, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 13'),
(108, 13, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 14'),
(108, 14, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 15'),
(108, 15, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 16'),
(108, 16, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 17'),
(108, 17, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 18'),
(108, 18, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 19'),
(108, 19, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 20'),
(108, 20, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 21'),
(108, 21, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 22'),
(108, 22, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 23'),
(108, 23, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 24'),
(108, 24, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 25'),
(108, 25, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 26'),
(108, 26, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 27'),
(108, 27, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 28'),
(108, 28, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 29'),
(108, 29, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 30'),
(108, 30, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 31'),
(108, 31, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 32'),
(108, 32, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 33'),
(108, 33, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 34'),
(108, 34, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 35'),
(108, 35, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 36'),
(108, 36, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 37'),
(108, 37, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 38'),
(108, 38, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 39'),
(108, 39, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 40'),
(108, 40, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 41'),
(108, 41, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 42'),
(108, 42, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 43'),
(108, 43, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 44'),
(108, 44, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 45'),
(108, 45, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 46'),
(108, 46, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 47'),
(108, 47, 'bit', 'standard', 'readwrite', NULL, 'direct', 0, 1, 'little', 'Output 48'),
(108, 48, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 1'),
(108, 49, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 2'),
(108, 50, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 3'),
(108, 51, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 4'),
(108, 52, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 5'),
(108, 53, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 6'),
(108, 54, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 7'),
(108, 55, 'bit', 'standard', 'readonly', NULL, 'direct', 0, 1, 'little', 'Input 8');
INSERT INTO `model_secure_state_mapping` (`fk_model_id`, `fk_standard_io_definition_id`, `fk_secure_state_io_definition_id`) VALUES
-- Model 104: Aggregated_Plant_IO_2
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=0 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=16 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=1 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=17 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=2 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=18 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=3 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=19 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=4 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=20 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=5 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=21 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=6 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=22 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=7 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=23 AND purpose='secure_state' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=8 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=24 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=9 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=25 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=10 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=26 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=11 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=27 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=12 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=28 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=13 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=29 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=14 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=30 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1)),
(104, (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=15 AND purpose='standard' AND io_type='bit' AND hardware_access='readwrite' LIMIT 1), (SELECT io_definition_id FROM model_io_definition WHERE fk_model_id=104 AND logical_address=31 AND purpose='secure_state' AND io_type='register' AND hardware_access='readwrite' LIMIT 1));


-- =================================================================================
-- AGGREGATED MODEL CHILDREN (Model Level)
-- Defines which child models compose each aggregated model, with slot ordering.
-- The slot_index order MUST match the connection_string order in the devices table.
-- =================================================================================

INSERT INTO `aggregated_model_children` (`fk_aggregated_model_id`, `slot_index`, `fk_child_model_id`) VALUES
-- Model 104: Aggregated_Plant_IO_2 -> children: Model 4 (ETH 8SD) at slot 0, Model 16 (Borrell 8SD) at slot 1
(104, 0, 4),
(104, 1, 16),

-- Model 105: Aggregated_Plant_IO_3 -> children: Model 4 (ETH 8SD) at slot 0
(105, 0, 4),

-- Model 106: Aggregated_IO -> children: Model 7 (iBC24SD) at slot 0, Model 5 (RS-485 I8SD) at slot 1
(106, 0, 7),
(106, 1, 5),

-- Model 107: Aggregated_IO_2 -> children: Model 4 (ETH 8SD) at slot 0, Model 5 (RS-485 I8SD) at slot 1
(107, 0, 4),
(107, 1, 5),

-- Model 108: Meta_Aggregated_IO_4 -> children: Model 106 (Aggregated_IO) at slot 0, Model 107 (Aggregated_IO_2) at slot 1, Model 17 (8ED) at slot 2
(108, 0, 106),
(108, 1, 107),
(108, 2, 17);


-- =================================================================================
-- DEVICE INSTANCES
-- =================================================================================

-- This is what the user would configure. This is what the PLC has.
-- Self-describing SPI devices (the program will fill fk_model_id at runtime)
INSERT INTO `devices` (`module_id`, `module_name`, `fk_model_id`, `channel`, `connection_string`, `address_on_channel`, `timeout_ms`) VALUES
(1, 'SPI Slot 0', 16, 'spi', 'embedded-spi', '0', NULL),
(2, 'SPI Slot 1', 17, 'spi', 'embedded-spi', '1', NULL),
  -- (3, 'SPI Slot 2', NULL, 'spi', 'embedded-spi', '2', NULL),
(4, 'SPI Slot 3', 18, 'spi', 'embedded-spi', '3', NULL),
(5, 'SPI Slot 4', 20, 'spi', 'embedded-spi', '4', NULL),
(6, 'SPI Slot 5', 19, 'spi', 'embedded-spi', '5', NULL),
(7, 'SPI Slot 6', 17, 'spi', 'embedded-spi', '6', NULL),
(8, 'SPI Slot 7', 16, 'spi', 'embedded-spi', '7', NULL),
 
-- Modbus TCP/IP modules.
-- Each has a unique IP:Port in 'connection_string'. 'address_on_channel' is the Modbus Unit ID.
(9, 'ETH-8ED', 3, 'tcp', '192.168.100.66:502', '1', 1000),
(10, 'ETH-8SD', 4, 'tcp', '192.168.100.65:502', '1', 1000),
 
-- Modbus RTU modules.
-- fk_model_id is specified here because these devices are not self-describing.
-- They share the same physical bus ('/dev/ttyAS5') but have unique slave IDs in 'address_on_channel'.
-- (11, 'RS-485 8ED', 6, 'rs485', '/dev/ttyAS5', '8', 1000),
(14, 'RS-485 iBC24SD', 7, 'rs485', '/dev/ttyAS5', '9', 1000),
(15, 'RS-485 I8SD', 5, 'rs485', '/dev/ttyAS5', '10', 1000),
 
-- Modbus RTU over TCP/IP modules.
  -- ('RS-485 8ED', 6, 'tcp', '192.168.100.64:10001', '8', 1000),
-- (14, 'RS-485 iBC24SD', 7, 'tcp', '192.168.100.64:10001', '9', 1000),
-- (15, 'RS-485 I8SD', 5, 'tcp', '192.168.100.64:10001', '10', 1000);
 
 
-- Aggregated modules. AGGREGATION
-- The connection_string lists child module INSTANCE IDs in the SAME slot order
-- as defined in aggregated_model_children above.
 
(16, 'Aggregated_Plant_IO_2', 104, 'aggregated', 'aggregator:10;8', 'aggregated_agg_3', NULL),
(17, 'Aggregated_Plant_IO_3', 105, 'aggregated', 'aggregator:10', 'aggregated_agg_4', NULL),
(18, 'Aggregated_IO', 106, 'aggregated', 'aggregator:14;15', 'meta_aggregated_agg_1', NULL),
 
(19, 'Aggregated_IO_2', 107, 'aggregated', 'aggregator:10;15', 'meta_aggregated_agg_2', NULL),
(20, 'Meta_Aggregated_IO_4', 108, 'aggregated', 'aggregator:18;19;2', 'meta_aggregated_agg_4', NULL);

-- =================================================================================
-- POPULATE module_io_config FOR ALL DEVICES
-- The trigger only fires when model_io_definition rows are inserted AFTER devices.
-- Since devices are inserted AFTER model_io_definition, we must populate manually.
-- =================================================================================
INSERT IGNORE INTO `module_io_config`
    (`fk_module_id`, `fk_io_definition_id`, `user_label`, `scale_factor`, `offset`, `visibility`, `visibility_mode`, `refresh_rate`, `sync`)
SELECT
    d.module_id,
    mid.io_definition_id,
    CONCAT(COALESCE(NULLIF(mid.default_io_label, ''), mid.io_type), ' (Dev ', d.module_id, ')'),
    1.0000,
    0.0000,
    'visible',
    'periodically',
    1,
    1
FROM devices d
JOIN model_io_definition mid ON d.fk_model_id = mid.fk_model_id;


-- =================================================================================
-- CUSTOM LABEL OVERRIDES
-- =================================================================================

-- MODULE 2 (SPI Slot 1 - 8 Inputs)
UPDATE module_io_config s
JOIN model_io_definition m ON s.fk_io_definition_id = m.io_definition_id
SET s.user_label = CASE m.logical_address
    WHEN 0 THEN 'Emergency Stop Button'
    WHEN 1 THEN 'Start Cycle Button'
    WHEN 2 THEN 'Light Curtain Sensor'
    WHEN 3 THEN 'System Pressure OK'
    WHEN 4 THEN 'Part in Position Sensor'
    WHEN 5 THEN 'Manual Mode Key Switch'
    WHEN 6 THEN 'Reset Button'
    WHEN 7 THEN 'Spare Input 1'
END
WHERE s.fk_module_id = 2 
  AND m.purpose = 'standard' 
  AND m.hardware_access = 'readonly';

-- MODULE 7 (SPI Slot 6 - 8 Inputs)
UPDATE module_io_config s
JOIN model_io_definition m ON s.fk_io_definition_id = m.io_definition_id
SET s.user_label = CASE m.logical_address
    WHEN 0 THEN 'Cylinder A Retracted Sensor'
    WHEN 1 THEN 'Cylinder A Extended Sensor'
    WHEN 2 THEN 'Cylinder B Retracted Sensor'
    WHEN 3 THEN 'Cylinder B Extended Sensor'
    WHEN 4 THEN 'Vacuum OK Sensor'
    WHEN 5 THEN 'Feeder Level Low'
    WHEN 6 THEN 'Feeder Level High'
    WHEN 7 THEN 'Spare Input 2'
END
WHERE s.fk_module_id = 7
  AND m.purpose = 'standard' 
  AND m.hardware_access = 'readonly';

-- MODULE 8 (SPI Slot 7 - 8 Outputs)
UPDATE module_io_config s
JOIN model_io_definition m ON s.fk_io_definition_id = m.io_definition_id
SET s.user_label = CASE m.logical_address
    WHEN 0 THEN 'Gripper Open/Close'
    WHEN 1 THEN 'Reject Piston Extend'
    WHEN 2 THEN 'Part Ejector Air Blast'
    WHEN 3 THEN 'Stack Light - Red'
    WHEN 4 THEN 'Stack Light - Amber'
    WHEN 5 THEN 'Stack Light - Green'
    WHEN 6 THEN 'Acoustic Alarm'
    WHEN 7 THEN 'Spare Output 1'
END
WHERE s.fk_module_id = 8
  AND m.purpose = 'standard' 
  AND m.hardware_access = 'readwrite';

-- MODULE 8 (Safe State Labels)
-- Note: Using a self-join to get the standard label to append " (Safe State)"
UPDATE module_io_config s_safe
JOIN model_io_definition m_safe ON s_safe.fk_io_definition_id = m_safe.io_definition_id
JOIN module_io_config s_std ON s_safe.fk_module_id = s_std.fk_module_id
JOIN model_io_definition m_std ON s_std.fk_io_definition_id = m_std.io_definition_id
SET s_safe.user_label = CONCAT(s_std.user_label, ' (Safe State)')
WHERE s_safe.fk_module_id = 8
  AND m_safe.purpose = 'secure_state'
  AND m_std.purpose = 'standard'
  AND m_safe.logical_address = m_std.logical_address;

-- MODULE 10 (ETH-8SD)
UPDATE module_io_config s
JOIN model_io_definition m ON s.fk_io_definition_id = m.io_definition_id
SET s.user_label = CONCAT('Network Valve Block A', m.logical_address + 1)
WHERE s.fk_module_id = 10
  AND m.purpose = 'standard' 
  AND m.hardware_access = 'readwrite';

-- MODULE 14 (RS-485 iBC24SD)
UPDATE module_io_config s
JOIN model_io_definition m ON s.fk_io_definition_id = m.io_definition_id
SET s.user_label = CONCAT('Main Panel Output ', m.logical_address + 1)
WHERE s.fk_module_id = 14
  AND m.purpose = 'standard' 
  AND m.hardware_access = 'readwrite';

-- MODULE 15 (RS-485 I8SD)
UPDATE module_io_config s
JOIN model_io_definition m ON s.fk_io_definition_id = m.io_definition_id
SET s.user_label = CONCAT('Auxiliary Panel Output ', m.logical_address + 1)
WHERE s.fk_module_id = 15
  AND m.purpose = 'standard' 
  AND m.hardware_access = 'readwrite';


-- =================================================================================
-- AGGREGATED I/O MAP (Model Level)
-- Maps aggregated I/O definitions to child MODEL I/O definitions using slot indexes.
-- Each row maps one aggregated I/O point to a specific child model's I/O point.
-- The child_slot_index references the slot order from aggregated_model_children.
-- =================================================================================

-- =================== MAPPINGS FOR Model 104 (Aggregated_Plant_IO_2) ===================
-- Slots: 0=Model 4 (ETH 8SD), 1=Model 16 (Borrell 8SD)

-- Map first 8 standard outputs (0-7) to slot 0 (Model 4)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON v_def.logical_address = c_def.logical_address
    WHERE v_def.fk_model_id = 104 
    AND v_def.purpose = 'standard' 
    AND v_def.hardware_access = 'readwrite'
    AND v_def.logical_address < 8 
    AND c_def.fk_model_id = 4  
    AND c_def.purpose = 'standard' 
    AND c_def.hardware_access = 'readwrite';

-- Map next 8 standard outputs (8-15) to slot 1 (Model 16)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 1 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON v_def.logical_address = (c_def.logical_address + 8)
    WHERE v_def.fk_model_id = 104 
    AND v_def.purpose = 'standard' 
    AND v_def.hardware_access = 'readwrite'
    AND v_def.logical_address BETWEEN 8 AND 15 
    AND c_def.fk_model_id = 16  
    AND c_def.purpose = 'standard' 
    AND c_def.hardware_access = 'readwrite';

-- Map 8 secure_state outputs (16-23) to slot 1 (Model 16)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 1 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON v_def.logical_address = (c_def.logical_address + 8)
    WHERE v_def.fk_model_id = 104 
    AND v_def.purpose = 'secure_state' 
    AND v_def.hardware_access = 'readwrite' 
    AND v_def.logical_address BETWEEN 16 AND 23
    AND c_def.fk_model_id = 16  
    AND c_def.purpose = 'secure_state' 
    AND c_def.hardware_access = 'readwrite';

-- Map next 8 secure_state outputs (24-31) to slot 0 (Model 4)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON (v_def.logical_address + 3) = c_def.logical_address
    WHERE v_def.fk_model_id = 104
    AND v_def.purpose = 'secure_state'
    AND v_def.hardware_access = 'readwrite'
    AND v_def.logical_address BETWEEN 24 AND 31
    AND c_def.fk_model_id = 4
    AND c_def.purpose = 'secure_state'
    AND c_def.hardware_access = 'readwrite';

-- Map config WDT (logical 32) to slot 0 (Model 4) WDT (logical 19)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def 
      ON v_def.logical_address = 32 AND c_def.logical_address = 19
    WHERE v_def.fk_model_id = 104
    AND v_def.purpose = 'config'
    AND v_def.hardware_access = 'readwrite'
    AND c_def.fk_model_id = 4
    AND c_def.purpose = 'config'
    AND c_def.hardware_access = 'readwrite';


-- =================== MAPPINGS FOR Model 105 (Aggregated_Plant_IO_3) ===================
-- Slots: 0=Model 4 (ETH 8SD)

-- Map 8 standard outputs (0-7) to slot 0 (Model 4)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON v_def.logical_address = c_def.logical_address
    WHERE v_def.fk_model_id = 105 
    AND v_def.purpose = 'standard' 
    AND v_def.hardware_access = 'readwrite'
    AND v_def.logical_address < 8 
    AND c_def.fk_model_id = 4   
    AND c_def.purpose = 'standard' 
    AND c_def.hardware_access = 'readwrite';


-- =================== MAPPINGS FOR Model 106 (Aggregated_IO) ===================
-- Slots: 0=Model 7 (iBC24SD), 1=Model 5 (RS-485 I8SD)

-- Map first 24 outputs (0-23) to slot 0 (Model 7)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON v_def.logical_address = c_def.logical_address
    WHERE v_def.fk_model_id = 106 
    AND v_def.purpose = 'standard' 
    AND v_def.hardware_access = 'readwrite'
    AND v_def.logical_address < 24 
    AND c_def.fk_model_id = 7 
    AND c_def.purpose = 'standard' 
    AND c_def.hardware_access = 'readwrite';

-- Map next 8 outputs (24-31) to slot 1 (Model 5)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 1 AS child_slot_index, c_def.io_definition_id
    FROM model_io_definition AS v_def
    JOIN model_io_definition AS c_def ON v_def.logical_address = c_def.logical_address + 24
    WHERE v_def.fk_model_id = 106 
    AND v_def.purpose = 'standard' 
    AND v_def.hardware_access = 'readwrite'
    AND v_def.logical_address BETWEEN 24 AND 31 
    AND c_def.fk_model_id = 5 
    AND c_def.purpose = 'standard' 
    AND c_def.hardware_access = 'readwrite';


-- =================== MAPPINGS FOR Model 107 (Aggregated_IO_2) ===================
-- Slots: 0=Model 4 (ETH 8SD), 1=Model 5 (RS-485 I8SD)

-- Map first 8 standard outputs (0-7) to slot 0 (Model 4)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id 
    FROM model_io_definition v_def 
    JOIN model_io_definition c_def ON v_def.logical_address = c_def.logical_address
    WHERE v_def.fk_model_id=107 
    AND v_def.purpose='standard' 
    AND v_def.hardware_access='readwrite' 
    AND v_def.logical_address < 8 
    AND c_def.fk_model_id=4 
    AND c_def.purpose='standard' 
    AND c_def.hardware_access='readwrite';

-- Map final 8 standard outputs (8-15) to slot 1 (Model 5)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 1 AS child_slot_index, c_def.io_definition_id 
    FROM model_io_definition v_def 
    JOIN model_io_definition c_def ON v_def.logical_address = c_def.logical_address + 8
    WHERE v_def.fk_model_id=107 
    AND v_def.purpose='standard' 
    AND v_def.hardware_access='readwrite'
    AND v_def.logical_address BETWEEN 8 AND 15 
    AND c_def.fk_model_id=5 
    AND c_def.purpose='standard' 
    AND c_def.hardware_access='readwrite';


-- =================== MAPPINGS FOR Model 108 (Meta_Aggregated_IO_4) ===================
-- Slots: 0=Model 106 (Aggregated_IO), 1=Model 107 (Aggregated_IO_2), 2=Model 17 (8ED)

-- Map first 32 standard outputs (0-31) to slot 0 (Model 106)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 0 AS child_slot_index, c_def.io_definition_id 
    FROM model_io_definition v_def 
    JOIN model_io_definition c_def 
    ON v_def.logical_address = c_def.logical_address
    WHERE v_def.fk_model_id=108 
    AND v_def.purpose='standard' 
    AND v_def.hardware_access='readwrite'
    AND v_def.logical_address < 32 
    AND c_def.fk_model_id=106 
    AND c_def.purpose='standard' 
    AND c_def.hardware_access='readwrite';

-- Map next 16 standard outputs (32-47) to slot 1 (Model 107)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 1 AS child_slot_index, c_def.io_definition_id 
    FROM model_io_definition v_def 
    JOIN model_io_definition c_def 
    ON v_def.logical_address = c_def.logical_address + 32
    WHERE v_def.fk_model_id=108 
    AND v_def.purpose='standard' 
    AND v_def.hardware_access='readwrite'
    AND v_def.logical_address BETWEEN 32 AND 47 
    AND c_def.fk_model_id=107 
    AND c_def.purpose='standard' 
    AND c_def.hardware_access='readwrite';

-- Map 8 standard inputs (48-55) to slot 2 (Model 17)
INSERT INTO `aggregated_io_map` (fk_aggregated_io_definition_id, child_slot_index, fk_child_io_definition_id)
    SELECT v_def.io_definition_id, 2 AS child_slot_index, c_def.io_definition_id 
    FROM model_io_definition v_def 
    JOIN model_io_definition c_def ON v_def.logical_address = c_def.logical_address + 48
    WHERE v_def.fk_model_id=108 
    AND v_def.purpose='standard' 
    AND v_def.hardware_access='readonly' 
    AND v_def.logical_address BETWEEN 48 AND 55 
    AND c_def.fk_model_id=17 
    AND c_def.purpose='standard' 
    AND c_def.hardware_access='readonly';

-- =================================================================================
-- INITIAL CONFIGURATION UPDATES
-- =================================================================================

-- Set default units to 'kV' for all Standard Registers
UPDATE module_io_config cfg
JOIN model_io_definition mid ON cfg.fk_io_definition_id = mid.io_definition_id
SET cfg.units = 'kV'
WHERE mid.io_type = 'register' 
AND mid.purpose = 'standard';
