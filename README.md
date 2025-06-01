# Database Testing Toolkit - Equipment Tracker

A comprehensive C-based equipment tracking application with advanced database management capabilities, developed using Claude Code AI assistance.

## Overview

This equipment tracker provides a robust solution for managing equipment inventory with features including:
- Equipment creation, updating, and deletion
- Database backup and restoration
- Transaction logging
- Memory management optimization
- Comprehensive error handling

## Development Process

This application was enhanced and refined using **Claude Code**, Anthropic's AI-powered development assistant. Claude Code helped with:

- **Code Analysis**: Reviewing the original codebase for potential improvements
- **Memory Management**: Implementing proper malloc/free patterns and error handling
- **Database Operations**: Optimizing SQLite operations and transaction handling
- **Code Refactoring**: Restructuring code for better maintainability and performance
- **Testing Strategy**: Developing comprehensive test scenarios and validation approaches
- **Documentation**: Creating clear, structured documentation and setup instructions

The iterative development process with Claude Code enabled rapid prototyping, bug fixes, and feature enhancements while maintaining code quality and best practices.

## Database Setup for Testing

### Prerequisites
- GCC compiler
- SQLite3 development libraries
- Basic terminal/command line knowledge

### Test Database Configuration

**IMPORTANT**: Use these test credentials only for development and testing purposes:

```
Database Name: test_equipment_inventory.db
Test Admin User: testadmin
Test Admin Password: TestPass123!
Test Regular User: testuser
Test Regular Password: UserPass456!

Test Equipment Categories:
- Electronics
- Furniture  
- Vehicles
- Tools
- Safety Equipment
```

### Setup Instructions

1. **Compile the Application**
   ```bash
   gcc -o equipment_tracker equipment_tracker_enhanced.c -lsqlite3
   ```

2. **Install SQLite3 (if not already installed)**
   
   Ubuntu/Debian:
   ```bash
   sudo apt-get install sqlite3 libsqlite3-dev
   ```
   
   CentOS/RHEL:
   ```bash
   sudo yum install sqlite-devel
   ```
   
   macOS:
   ```bash
   brew install sqlite3
   ```

3. **Initialize Test Database**
   ```bash
   ./equipment_tracker
   ```
   
   The application will automatically create the database file and required tables on first run.

4. **Test Data Population**
   
   Use the application menu to add test equipment:
   - Sample laptops, desks, vehicles
   - Test different categories and conditions
   - Verify backup/restore functionality

### Testing Scenarios

1. **Basic CRUD Operations**
   - Create equipment entries
   - Read/search equipment
   - Update equipment details
   - Delete equipment records

2. **Database Management**
   - Create database backups
   - Restore from backup files
   - Verify data integrity after operations

3. **Error Handling**
   - Test with invalid inputs
   - Simulate database connection issues
   - Verify memory cleanup on errors

4. **Performance Testing**
   - Add large numbers of equipment entries
   - Test search performance
   - Monitor memory usage during operations

### Security Notes

- The test credentials provided are for development only
- Never use test credentials in production environments
- Implement proper authentication and authorization for production use
- Consider encrypting sensitive data in production databases

### Configuration Files

The application uses `db_config.conf` for database configuration. Ensure this file contains appropriate test settings:

```
db_path=test_equipment_inventory.db
backup_path=./backups/
log_level=DEBUG
```

## Features

- **Equipment Management**: Full CRUD operations for equipment tracking
- **Database Backup**: Automated backup and restore capabilities  
- **Transaction Logging**: Comprehensive logging of all database operations
- **Memory Safety**: Proper memory management with leak prevention
- **Error Recovery**: Robust error handling and recovery mechanisms

## Usage

Run the compiled executable and follow the interactive menu system to manage your equipment inventory.

## Contributing

This project was developed with AI assistance from Claude Code. Future enhancements and contributions should maintain the established code quality and documentation standards.