Feature: filter accounts

    As a user
    I want to filter the accounts
    So that I can view the data of the filtered accounts

    Scenario: synced files can be filter based on user account
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has created folder "simple-folder" on the server
        And user "Alice" has set up a client with default settings
        And the user has added another account with
            | server   | %local_server% |
            | user     | Brian          |
            | password | AaBb2Cc3Dd4    |
        When the user clicks on the activity tab
        And the user selects "Alice Hansen@localhost" option on filter dropdown button
        Then the following information should be displayed on the sync table
        	|action    |resource		   |account                 |
            |Downloaded|simple-folder      |Alice Hansen@localhost  |