using System;
using System.ComponentModel.DataAnnotations;
using McMaster.Extensions.CommandLineUtils;
using McMaster.Extensions.CommandLineUtils.Validation;

namespace LargeDatasetGenerator.SyncGateway
{
    internal sealed class UrlValidation : IOptionValidator
    {
        public ValidationResult GetValidationResult(CommandOption option, ValidationContext context)
        {
            return Uri.TryCreate(option.Value(), UriKind.Absolute, out var uri)
                ? ValidationResult.Success
                : new ValidationResult("--url must be a valid URL");
        }
    }
}