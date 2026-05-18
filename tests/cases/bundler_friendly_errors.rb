$stdout.sync = true

require "bundler/friendly_errors"

value = Bundler::FriendlyErrors.with_friendly_errors do
  42
end
puts value

ran = false
Bundler::FriendlyErrors.with_friendly_errors do
  ran = true
end
puts ran
puts Bundler::FriendlyErrors.exit_status(Bundler::GemNotFound.new("missing"))
puts Bundler::FriendlyErrors.exit_status(Bundler::GemfileNotFound.new("no gemfile"))
puts Bundler::FriendlyErrors.friendly_error_message(Bundler::GemNotFound.new("demo missing"))
